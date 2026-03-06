# Shader Uniform Management

This project uses a standardized and optimized approach to managing shader uniforms, focusing on reducing boilerplate, ensuring type safety, and improving initialization speed.

## 1. Batch Uniform Initialization

Instead of manually calling `shader_get_uniform_location` for every single uniform, we use the `shader_init_uniforms` helper.

### The Problem: Boilerplate Overload

Standard OpenGL uniform initialization often looks like this:

```c
struct MyUniforms {
    GLint u_time;
    GLint u_resolution;
    GLint u_mouse;
};

// ...
uniforms.u_time = glGetUniformLocation(program, "u_time");
uniforms.u_resolution = glGetUniformLocation(program, "u_resolution");
uniforms.u_mouse = glGetUniformLocation(program, "u_mouse");
```

This is error-prone and tedious for shaders with dozens of uniforms.

### The Solution: `shader_init_uniforms`

We define an array of names and a destination structure, and the helper initializes everything in one call.

**Example from `src/pbr.c`**:

```c
void pbr_get_spec_uniforms(GLuint shader, PBRSpecUniforms* out)
{
    static const char* names[] = {
        "envMap", "roughnessValue", "currentMipLevel",
        "clampThreshold", "u_offset_y", "u_max_y"
    };
    shader_init_uniforms(shader, names, (GLint*)out,
                         sizeof(names) / sizeof(names[0]));
}
```

> [!IMPORTANT]
> **Strict Field Order**: For this to work safely, the order of names in the `names` array **MUST EXACTLY MATCH** the order of `GLint` fields in the target structure.

## 2. Dynamic Uniform Retrieval

For uniforms that are not part of a static structure (e.g., arrays with dynamic indices like SH textures), we still use the standard `shader_get_uniform_location` wrapped in the `Shader` object's caching system.

```c
for (int i = 0; i < SH_TEXTURE_COUNT; i++) {
    char name[32];
    safe_snprintf(name, sizeof(name), "u_SHTexture%d", i);
    scene->instanced_uniforms.sh_textures[i] =
        shader_get_uniform_location(shader, name);
}
```

## 3. Benefits

1. **Readability**: uniform initialization is reduced to a single declarative block.
2. **Maintainability**: Adding a new uniform only requires adding a field to the struct and a string to the `names` array.
3. **Safety**: Reduces the risk of typos in uniform names and ensures all locations are retrieved consistently.
4. **Performance**: The helper is highly optimized and minimizes JNI-style overhead when retrieving multiple locations.

## 4. Best Practices

- **Group Related Uniforms**: Use specialized structures (e.g., `PostProcessUniforms`, `SceneUniforms`) to keep related data together.
- **Use Null Checks**: Always check if a retrieved uniform location is `-1` before using it with `glUniform*` if the uniform might be optimized out by the shader compiler.
- **Cache Locations**: Never call `glGetUniformLocation` in a render loop; always use the cached locations in the structures.
