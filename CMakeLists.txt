cmake_minimum_required(VERSION 3.14)
project(suckless-ogl C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Source files
set(SOURCES
    src/main.c
    src/app.c
    src/icosphere.c
    src/shader.c
    src/texture.c
    src/skybox.c
    src/log.c
)

# Silence warnings about legacy CMake versions in subprojects (e.g., cglm, glad)
set(CMAKE_WARN_DEPRECATED OFF CACHE BOOL "" FORCE)

# Dependencies (automated via FetchContent)
include(FetchContent)
# GLFW, CGLM, GLAD are handled below.

# 1. CGLM (Static Build)
message(STATUS "Fetching cglm...")
FetchContent_Declare(
    cglm
    GIT_REPOSITORY https://github.com/recp/cglm.git
    GIT_TAG        v0.9.4
)
# Force cglm to be built as a static library
set(CGLM_STATIC ON CACHE BOOL "Build cglm as static library" FORCE)
FetchContent_MakeAvailable(cglm)

# 2. GLAD (Automatic Generation)
message(STATUS "Fetching glad...")
FetchContent_Declare(
    glad
    GIT_REPOSITORY https://github.com/Dav1dde/glad.git
    GIT_TAG        v0.1.36
)
# Configure GLAD before making available
set(GLAD_PROFILE "core" CACHE STRING "OpenGL profile" FORCE)
set(GLAD_API "gl=4.4" CACHE STRING "API type/version pairs" FORCE)
set(GLAD_GENERATOR "c" CACHE STRING "Language to generate" FORCE)

FetchContent_MakeAvailable(glad)

# 3. GLFW (System Package)
find_package(PkgConfig REQUIRED)
pkg_check_modules(GLFW3 REQUIRED glfw3)

# App target
add_executable(app ${SOURCES})

# Target properties and includes
target_include_directories(app PRIVATE src include)
target_link_libraries(app PRIVATE cglm glad ${GLFW3_LIBRARIES} m dl)
target_compile_definitions(app PRIVATE GLFW_INCLUDE_NONE)

# Compiler flags
if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(app PRIVATE -Wall -Wextra -O2)
endif()
