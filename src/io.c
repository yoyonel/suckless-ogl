#include "io.h"

#include "log.h"
#include "utils.h"
#include <stdio.h>

/* DEFAULT_MAX_FILE_SIZE (16MB) based on previous shader limit */
enum { DEFAULT_MAX_FILE_SIZE = 16 * 1024 * 1024 };

char* io_read_file(const char* path, size_t max_size, size_t* out_size)
{
	if (!is_safe_relative_path(path)) {
		LOG_ERROR("suckless-ogl.io",
		          "Security Violation: Unsafe path blocked: %s", path);
		return NULL;
	}

	CLEANUP_FILE FILE* file_ptr = fopen(path, "rb");
	if (!file_ptr) {
		LOG_ERROR("suckless-ogl.io", "Failed to open file: %s", path);
		return NULL;
	}

	if (fseek(file_ptr, 0, SEEK_END) != 0) {
		LOG_ERROR("suckless-ogl.io", "Failed to seek end: %s", path);
		RAII_SATISFY_FILE(file_ptr);
		return NULL;
	}

	long len = ftell(file_ptr);
	if (len < 0) {
		LOG_ERROR("suckless-ogl.io", "Failed to tell size: %s", path);
		RAII_SATISFY_FILE(file_ptr);
		return NULL;
	}

	size_t limit =
	    (max_size > 0) ? max_size : (size_t)DEFAULT_MAX_FILE_SIZE;
	if ((size_t)len > limit) {
		LOG_ERROR("suckless-ogl.io", "File too large: %s (%ld > %zu)",
		          path, len, limit);
		RAII_SATISFY_FILE(file_ptr);
		return NULL;
	}

	size_t size = (size_t)len;
	CLEANUP_FREE char* buf = safe_calloc(size + 1, 1);
	if (!buf) {
		LOG_ERROR("suckless-ogl.io", "Allocation failed: %s", path);
		RAII_SATISFY_FILE(file_ptr);
		return NULL;
	}

	if (fseek(file_ptr, 0, SEEK_SET) != 0) {
		LOG_ERROR("suckless-ogl.io", "Failed to seek set: %s", path);
		RAII_SATISFY_FILE(file_ptr);
		RAII_SATISFY_FREE(buf);
		return NULL;
	}

	size_t read_count = fread(buf, 1, size, file_ptr);
	if (read_count != size) {
		LOG_ERROR("suckless-ogl.io", "Incomplete read: %s", path);
		RAII_SATISFY_FILE(file_ptr);
		RAII_SATISFY_FREE(buf);
		return NULL;
	}

	if (out_size) {
		*out_size = size;
	}

	RAII_SATISFY_FILE(file_ptr);
	return TRANSFER_OWNERSHIP(buf);
}
