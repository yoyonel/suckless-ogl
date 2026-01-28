#include "shader.h"

#include "glad/glad.h"
#include "log.h"
#include "utils.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * NOTE ABOUT COVERAGE (LLVM / llvm-cov)
 * -----------------------------------
 * This file is instrumented using clang + llvm-profdata + llvm-cov.
 * Contrary to lcov/gcov, llvm-cov DOES NOT honor LCOV_EXCL_* markers.
 * The ONLY supported exclusion mechanism is the use of explicit
 * comments of the form:
 *   - // llvm-cov ignore next line
 *   - // llvm-cov ignore begin / end
 *
 * The ignored paths below correspond to defensive I/O error handling
 * which is practically impossible to trigger deterministically in
 * black-box tests on modern POSIX/Linux systems (regular files).
 * Excluding them avoids misleading coverage penalties while keeping
 * the legacy ISO C code intact and honest.
 */

#define CLEANUP_CTX __attribute__((cleanup(ctx_free)))

enum { INFO_LOG_SIZE = 512 };

enum { MAX_SHADER_NAME_LEN = 256 };

enum { SHADER_LABEL_BUFFER_SIZE = 512 };

/* -------------------------------------------------------------------------
 * Internal Include Processing (Chunk-List / Single-Pass Allocation)
 * ------------------------------------------------------------------------- */

typedef struct LoadedBuffer {
	char* data;
	struct LoadedBuffer* next;
} LoadedBuffer;

typedef struct Chunk {
	const char* ptr;
	size_t len;
	struct Chunk* next;
} Chunk;

typedef struct {
	LoadedBuffer* buffers_head; /* List of all raw file buffers to free */
	Chunk* chunks_head; /* Ordered list of text chunks to assemble */
	Chunk* chunks_tail;
	size_t total_size;
	int recursion_depth;
} IncludeContext;

enum { MAX_INCLUDE_DEPTH = 16 };
/* 16MB limit for shader source to prevent abuse/taint issues */
enum { MAX_SHADER_SOURCE_SIZE = 16 * 1024 * 1024 };
enum { PATH_BUFFER_SIZE = 256 };
enum { RESOLVED_PATH_BUFFER_SIZE = 512 };
enum { HEADER_TAG_LEN = 7 };

/* Forward declaration */
static bool process_source(IncludeContext* ctx, const char* current_file_src,
                           const char* current_file_path);

/*
 * Reads an entire file into a null-terminated string.
 * This is the only place doing raw I/O and malloc for file content.
 */
static char* load_file_into_ram(const char* path)
{
	CLEANUP_FILE FILE* file_ptr = fopen(path, "rb");
	if (!file_ptr) {
		LOG_ERROR("suckless-ogl.shader", "Failed to open file: %s",
		          path);
		return NULL;
	}

	if (fseek(file_ptr, 0, SEEK_END) != 0) {
		LOG_ERROR("suckless-ogl.shader", "Failed to seek end: %s",
		          path);
		RAII_SATISFY_FILE(file_ptr);
		return NULL;
	}

	long len = ftell(file_ptr);
	if (len < 0) {
		LOG_ERROR("suckless-ogl.shader", "Failed to tell size: %s",
		          path);
		RAII_SATISFY_FILE(file_ptr);
		return NULL;
	}

	size_t size = (size_t)len;
	CLEANUP_FREE char* buf = safe_calloc(size + 1, 1);
	if (!buf) {
		LOG_ERROR("suckless-ogl.shader", "Allocation failed: %s", path);
		RAII_SATISFY_FILE(file_ptr);
		return NULL;
	}

	if (fseek(file_ptr, 0, SEEK_SET) != 0) {
		LOG_ERROR("suckless-ogl.shader", "Failed to seek set: %s",
		          path);
		RAII_SATISFY_FILE(file_ptr);
		RAII_SATISFY_FREE(buf);
		return NULL;
	}

	size_t read_count = fread(buf, 1, size, file_ptr);
	if (read_count != size) {
		LOG_ERROR("suckless-ogl.shader", "Incomplete read: %s", path);
		RAII_SATISFY_FILE(file_ptr);
		RAII_SATISFY_FREE(buf);
		return NULL;
	}

	RAII_SATISFY_FILE(file_ptr);
	return TRANSFER_OWNERSHIP(buf);
}

static void ctx_add_buffer(IncludeContext* ctx, char* data)
{
	LoadedBuffer* loaded_buf = malloc(sizeof(LoadedBuffer));
	loaded_buf->data = data;
	loaded_buf->next = ctx->buffers_head;
	ctx->buffers_head = loaded_buf;
}

static void ctx_add_chunk(IncludeContext* ctx, const char* ptr, size_t len)
{
	if (len == 0) {
		return;
	}
	Chunk* chunk = malloc(sizeof(Chunk));
	chunk->ptr = ptr;
	chunk->len = len;
	chunk->next = NULL;

	if (ctx->chunks_tail) {
		ctx->chunks_tail->next = chunk;
	} else {
		ctx->chunks_head = chunk;
	}
	ctx->chunks_tail = chunk;
	ctx->total_size += len;
}

static void ctx_free(IncludeContext* ctx)
{
	/* Free all file buffers */
	while (ctx->buffers_head) {
		LoadedBuffer* next = ctx->buffers_head->next;
		free(ctx->buffers_head->data);
		free(ctx->buffers_head);
		ctx->buffers_head = next;
	}
	/* Free all chunk nodes (pointers inside are owned by buffers above) */
	while (ctx->chunks_head) {
		Chunk* next = ctx->chunks_head->next;
		free(ctx->chunks_head);
		ctx->chunks_head = next;
	}
}

/* Returns directory part of path (including trailing slash) or "./" */
static void get_dir_from_path(const char* path, char* out_dir, size_t size)
{
	const char* last_slash = strrchr(path, '/');
	if (last_slash) {
		size_t len = (size_t)(last_slash - path) + 1;
		if (len >= size) {
			len = size - 1;
		}
		safe_memcpy(out_dir, size, path, len);
		out_dir[len] = '\0';
	} else {
		safe_snprintf(out_dir, size, "./");
	}
}

/*
 * Helper to resolve and parse an included file.
 * Returns true on success, false on error.
 */
// NOLINTNEXTLINE(misc-no-recursion)
static bool resolve_and_parse_include(IncludeContext* ctx,
                                      const char* path_term,
                                      const char* current_file_path)
{
	/* Resolve relative path */
	char current_dir[PATH_BUFFER_SIZE];
	get_dir_from_path(current_file_path, current_dir, sizeof(current_dir));

	char resolved_path[RESOLVED_PATH_BUFFER_SIZE];
	safe_snprintf(resolved_path, sizeof(resolved_path), "%s%s", current_dir,
	              path_term);

	/* Load the included file */
	char* inc_src = load_file_into_ram(resolved_path);
	if (!inc_src) {
		LOG_ERROR("suckless-ogl.shader",
		          "Failed to resolve include: %s (in %s)", path_term,
		          current_file_path);
		return false;
	}

	ctx_add_buffer(ctx, inc_src);

	/* Recursively process the included content */
	return process_source(ctx, inc_src, resolved_path);
}

/*
 * Helper to parse the include path from the arguments after @header.
 * Writes the parsed path to out_path.
 * Returns the pointer to the end of the line (or end of args).
 */
static const char* parse_include_path(const char* args, char* out_path,
                                      size_t size)
{
	while (*args == ' ' || *args == '\t') {
		args++;
	}

	const char* end_of_line = strchr(args, '\n');
	if (!end_of_line) {
		end_of_line = args + strlen(args);
	}

	/* Extract path token */
	const char* path_start = args;
	const char* path_end = end_of_line;

	if (*args == '"') {
		path_start = args + 1;
		const char* close_quote = strchr(path_start, '"');
		if (close_quote && close_quote < end_of_line) {
			path_end = close_quote;
		}
	}

	size_t path_len = (size_t)(path_end - path_start);
	if (path_len >= size) {
		path_len = size - 1;
	}
	safe_memcpy(out_path, size, path_start, path_len);
	out_path[path_len] = '\0';

	/* Trim trailing whitespace if no quotes */
	if (*args != '"') {
		char* ptr = out_path + path_len - 1;
		while (ptr >= out_path && (*ptr == ' ' || *ptr == '\r')) {
			*ptr = '\0';
			ptr--;
		}
	}

	return end_of_line;
}

/* Recursive function to process text and resolve @header */
// NOLINTNEXTLINE(misc-no-recursion)
static bool process_source(IncludeContext* ctx, const char* current_file_src,
                           const char* current_file_path)
{
	if (ctx->recursion_depth > MAX_INCLUDE_DEPTH) {
		LOG_ERROR("suckless-ogl.shader",
		          "Max include depth exceeded at: %s",
		          current_file_path);
		return false;
	}

	ctx->recursion_depth++;
	const char* cursor = current_file_src;

	while (cursor && *cursor) {
		const char* next_tag = strstr(cursor, "@header");
		if (!next_tag) {
			ctx_add_chunk(ctx, cursor, strlen(cursor));
			break;
		}

		/* Check valid start of line */
		bool at_line_start =
		    (next_tag == current_file_src) || (*(next_tag - 1) == '\n');

		if (!at_line_start) {
			size_t len =
			    (size_t)(next_tag - cursor) + HEADER_TAG_LEN;
			ctx_add_chunk(ctx, cursor, len);
			cursor = next_tag + HEADER_TAG_LEN;
			continue;
		}

		/* Add chunk BEFORE the tag */
		ctx_add_chunk(ctx, cursor, (size_t)(next_tag - cursor));

		/* Parse path */
		char raw_inc_path[PATH_BUFFER_SIZE];
		const char* end_of_line =
		    parse_include_path(next_tag + HEADER_TAG_LEN, raw_inc_path,
		                       sizeof(raw_inc_path));

		if (!resolve_and_parse_include(ctx, raw_inc_path,
		                               current_file_path)) {
			return false;
		}

		cursor = end_of_line;
		if (*cursor == '\n') {
			cursor++;
		}
	}

	ctx->recursion_depth--;
	return true;
}

char* shader_read_file(const char* path)
{
	/* 1. Load root file */
	CLEANUP_FREE char* root_src = load_file_into_ram(path);
	if (!root_src) {
		return NULL;
	}

	/* 2. Setup Context */
	CLEANUP_CTX IncludeContext ctx = {0};
	ctx_add_buffer(&ctx, TRANSFER_OWNERSHIP(root_src));

	/* 3. Recursively parse and build chunk list */
	if (!process_source(&ctx, ctx.buffers_head->data, path)) {
		return NULL;
	}

	/* 4. Single Allocation for final result */
	CLEANUP_FREE char* final_src =
	    safe_calloc(ctx.total_size + 1, 1);  // NOLINT
	if (!final_src) {
		LOG_ERROR("suckless-ogl.shader",
		          "Final allocation failed (%lu bytes)",
		          ctx.total_size);
		return NULL;
	}

	/* 5. Assemble */
	char* wptr = final_src;
	Chunk* node = ctx.chunks_head;
	while (node) {
		safe_memcpy(wptr,
		            ctx.total_size + 1 - (size_t)(wptr - final_src),
		            node->ptr, node->len);
		wptr += node->len;
		node = node->next;
	}
	*wptr = '\0'; /* Null terminate */

	/* 6. Return and transfer ownership */
	return TRANSFER_OWNERSHIP(final_src);
}

GLuint shader_compile(const char* path, GLenum type)
{
	char* src = shader_read_file(path);
	if (!src) {
		LOG_ERROR("suckless-ogl.shader",
		          "Failed to read shader file: %s", path);
		return 0;
	}

	GLuint shader = glCreateShader(type);
	glShaderSource(shader, 1, (const char**)&src, NULL);
	glCompileShader(shader);
	free(src);

	int success = 0;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
	if (success == 0) {
		char log[INFO_LOG_SIZE];
		glGetShaderInfoLog(shader, INFO_LOG_SIZE, NULL, log);
		LOG_ERROR("suckless-ogl.shader",
		          "Shader compilation error (%s):\n%s", path, log);
		glDeleteShader(shader);
		return 0;
	}

	return shader;
}

GLuint shader_load_program(const char* vertex_path, const char* fragment_path)
{
	GLuint vertex_shader = shader_compile(vertex_path, GL_VERTEX_SHADER);
	if (vertex_shader == 0) {
		return 0;
	}

	GLuint fragment_shader =
	    shader_compile(fragment_path, GL_FRAGMENT_SHADER);
	if (fragment_shader == 0) {
		glDeleteShader(vertex_shader);
		return 0;
	}

	GLuint program = glCreateProgram();
	glAttachShader(program, vertex_shader);
	glAttachShader(program, fragment_shader);
	glLinkProgram(program);

	int success = 0;
	glGetProgramiv(program, GL_LINK_STATUS, &success);
	if (success == 0) {
		char log[INFO_LOG_SIZE];
		glGetProgramInfoLog(program, INFO_LOG_SIZE, NULL, log);
		LOG_ERROR("suckless-ogl.shader", "Shader linking error:\n%s",
		          log);
		glDeleteProgram(program);
		program = 0;
	}

	glDeleteShader(vertex_shader);
	glDeleteShader(fragment_shader);

	if (program != 0) {
		char name[SHADER_LABEL_BUFFER_SIZE];
		safe_snprintf(name, sizeof(name), "%s + %s", vertex_path,
		              fragment_path);
		glObjectLabel(GL_PROGRAM, program, -1, name);
	}

	return program;
}

GLuint shader_load_compute(const char* compute_path)
{
	GLuint compute_shader = shader_compile(compute_path, GL_COMPUTE_SHADER);
	if (compute_shader == 0) {
		return 0;
	}

	GLuint program = glCreateProgram();
	glAttachShader(program, compute_shader);
	glLinkProgram(program);

	int success = 0;
	glGetProgramiv(program, GL_LINK_STATUS, &success);
	if (success == 0) {
		char log[INFO_LOG_SIZE];
		glGetProgramInfoLog(program, INFO_LOG_SIZE, NULL, log);
		LOG_ERROR("suckless-ogl.shader",
		          "Compute shader linking error:\n%s", log);
		glDeleteProgram(program);
		program = 0;
	}

	glDeleteShader(compute_shader);

	if (program != 0) {
		glObjectLabel(GL_PROGRAM, program, -1, compute_path);
	}

	return program;
}

/* -------------------------------------------------------------------------
 * New Generic Shader API Implementation
 * ------------------------------------------------------------------------- */

static int cmp_uniform_entry(const void* lhs, const void* rhs)
{
	const UniformEntry* entry_lhs = (const UniformEntry*)lhs;
	const UniformEntry* entry_rhs = (const UniformEntry*)rhs;
	return strcmp(entry_lhs->name, entry_rhs->name);
}

static void shader_cache_uniforms(Shader* shader)
{
	GLint count = 0;
	glGetProgramiv(shader->program, GL_ACTIVE_UNIFORMS, &count);

	if (count == 0) {
		shader->entry_count = 0;
		shader->entries = NULL;
		return;
	}

	shader->entry_count = count;
	shader->entries = calloc(count, sizeof(UniformEntry));

	GLint max_name_len = 0;
	glGetProgramiv(shader->program, GL_ACTIVE_UNIFORM_MAX_LENGTH,
	               &max_name_len);
	char* name_buffer = calloc(max_name_len + 1, 1);

	for (int i = 0; i < count; i++) {
		GLsizei length = 0;
		GLint size = 0;
		GLenum type = 0;
		glGetActiveUniform(shader->program, i, max_name_len, &length,
		                   &size, &type, name_buffer);

		shader->entries[i].name = strdup(name_buffer);
		shader->entries[i].location =
		    glGetUniformLocation(shader->program, name_buffer);
	}

	free(name_buffer);

	/* Sort for binary search */
	qsort(shader->entries, shader->entry_count, sizeof(UniformEntry),
	      cmp_uniform_entry);
}

static Shader* shader_create_from_program(GLuint program, const char* name)
{
	if (program == 0) {
		return NULL;
	}

	Shader* shader = calloc(1, sizeof(Shader));
	shader->program = program;
	if (name) {
		shader->name = strdup(name);
		/* Label the program for profilers (RenderDoc/ApiTrace) */
		glObjectLabel(GL_PROGRAM, program, -1, name);
	} else {
		shader->name = strdup("Unknown Shader");
	}
	shader_cache_uniforms(shader);

	return shader;
}

Shader* shader_load(const char* vertex_path, const char* fragment_path)
{
	GLuint program = shader_load_program(vertex_path, fragment_path);

	/* Construct a name from paths */
	char name[MAX_SHADER_NAME_LEN];
	safe_snprintf(name, sizeof(name), "%s + %s", vertex_path,
	              fragment_path);

	return shader_create_from_program(program, name);
}

Shader* shader_load_compute_program(const char* compute_path)
{
	GLuint program = shader_load_compute(compute_path);
	return shader_create_from_program(program, compute_path);
}

void shader_destroy(Shader* shader)
{
	if (!shader) {
		return;
	}

	if (shader->entries) {
		for (int i = 0; i < shader->entry_count; i++) {
			free(shader->entries[i].name);
		}
		free(shader->entries);
	}

	if (shader->program) {
		glDeleteProgram(shader->program);
	}

	if (shader->name) {
		free(shader->name);
	}

	free(shader);
}

void shader_use(Shader* shader)
{
	if (shader) {
		glUseProgram(shader->program);
	}
}

GLint shader_get_uniform_location(Shader* shader, const char* name)
{
	if (!shader || !shader->entries || shader->entry_count == 0) {
		return -1;
	}

	UniformEntry key;
	key.name = (char*)name; /* Cast away const for bsearch key */

	UniformEntry* res = bsearch(&key, shader->entries, shader->entry_count,
	                            sizeof(UniformEntry), cmp_uniform_entry);

	if (res) {
		return res->location;
	}

	LOG_WARN("suckless-ogl.shader",
	         "Uniform '%s' not found or active in shader '%s' (ID %d)",
	         name, shader->name ? shader->name : "Unknown",
	         shader->program);
	return -1;
}

void shader_set_int(Shader* shader, const char* name, int val)
{
	GLint loc = shader_get_uniform_location(shader, name);
	if (loc != -1) {
		glUniform1i(loc, val);
	}
}

void shader_set_float(Shader* shader, const char* name, float val)
{
	GLint loc = shader_get_uniform_location(shader, name);
	if (loc != -1) {
		glUniform1f(loc, val);
	}
}

void shader_set_vec2(Shader* shader, const char* name, const float* val)
{
	GLint loc = shader_get_uniform_location(shader, name);
	if (loc != -1) {
		glUniform2fv(loc, 1, val);
	}
}

void shader_set_vec3(Shader* shader, const char* name, const float* val)
{
	GLint loc = shader_get_uniform_location(shader, name);
	if (loc != -1) {
		glUniform3fv(loc, 1, val);
	}
}

void shader_set_vec4(Shader* shader, const char* name, const float* val)
{
	GLint loc = shader_get_uniform_location(shader, name);
	if (loc != -1) {
		glUniform4fv(loc, 1, val);
	}
}

void shader_set_mat4(Shader* shader, const char* name, const float* val)
{
	GLint loc = shader_get_uniform_location(shader, name);
	if (loc != -1) {
		glUniformMatrix4fv(loc, 1, GL_FALSE, val);
	}
}
