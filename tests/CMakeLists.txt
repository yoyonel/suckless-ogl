# Test configuration

# Link against the project sources but ideally we should have a library target
# For now we just add the source files directly to the tests

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/src)

# 1. Test Icosphere
add_executable(test_icosphere test_icosphere.c ${CMAKE_SOURCE_DIR}/src/icosphere.c)
target_link_libraries(test_icosphere PRIVATE unity cglm m)
add_test(NAME TestIcosphere COMMAND test_icosphere)

# 2. Test Shader
# Needs shader.c and log.c
add_executable(test_shader test_shader.c ${CMAKE_SOURCE_DIR}/src/shader.c ${CMAKE_SOURCE_DIR}/src/log.c)
target_link_libraries(test_shader PRIVATE unity glad cglm m)
# Need POSIX defines for log.c
target_compile_definitions(test_shader PRIVATE _POSIX_C_SOURCE=199309L)
add_test(NAME TestShader COMMAND test_shader)

# 3. Test Log
add_executable(test_log test_log.c ${CMAKE_SOURCE_DIR}/src/log.c)
target_link_libraries(test_log PRIVATE unity)
# Need POSIX for log.c (clock_gettime) and test_log.c (dup/dup2)
target_compile_definitions(test_log PRIVATE _POSIX_C_SOURCE=199309L)
add_test(NAME TestLog COMMAND test_log)
