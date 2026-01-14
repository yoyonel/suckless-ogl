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
    src/stb_image_impl.c
)

# Silence warnings about legacy CMake versions in subprojects (e.g., cglm, glad)
set(CMAKE_WARN_DEPRECATED OFF CACHE BOOL "" FORCE)

# Dependencies (automated via FetchContent)
include(FetchContent)

# --- Offline Support Logic ---
# If a 'deps' directory exists, we check for subdirectories and use them as source overrides.
# This allows building offline if the dependencies are already cloned there.
set(OFFLINE_DEPS_DIR "${CMAKE_SOURCE_DIR}/deps")
if(EXISTS "${OFFLINE_DEPS_DIR}")
    message(STATUS "Offline mode: Checking for local dependencies in ${OFFLINE_DEPS_DIR}")
    
    # List of dependencies we want to support overrides for
    set(MANIFEST_DEPS "cglm" "glad" "stb" "unity")
    
    foreach(dep ${MANIFEST_DEPS})
        if(EXISTS "${OFFLINE_DEPS_DIR}/${dep}")
            message(STATUS "  Using local source for ${dep}: ${OFFLINE_DEPS_DIR}/${dep}")
            string(TOUPPER "${dep}" dep_upper)
            set(FETCHCONTENT_SOURCE_DIR_${dep_upper} "${OFFLINE_DEPS_DIR}/${dep}" CACHE PATH "Local source override for ${dep}")
        endif()
    endforeach()
    
    # Force FetchContent to not even try to connect if we are in offline mode
    # set(FETCHCONTENT_FULLY_DISCONNECTED ON CACHE BOOL "Forbid FetchContent from using network" FORCE)
    
    # GLAD specific: use the local specification file instead of downloading it
    set(GLAD_REPRODUCIBLE ON CACHE BOOL "Use local gl.xml for GLAD" FORCE)
endif()
# -----------------------------

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

# 3. STB (Header-only)
message(STATUS "Fetching stb...")
FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG        master # stb doesn't have regular tags, master is common
)
FetchContent_MakeAvailable(stb)

# 3. GLFW (System Package)
find_package(PkgConfig REQUIRED)
pkg_check_modules(GLFW3 REQUIRED glfw3)

# App target
add_executable(app ${SOURCES})

# Target properties and includes
target_include_directories(app PRIVATE src include ${stb_SOURCE_DIR})
target_link_libraries(app PRIVATE cglm glad ${GLFW3_LIBRARIES} m dl)
target_compile_definitions(app PRIVATE GLFW_INCLUDE_NONE)

# Compiler flags
if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(app PRIVATE -Wall -Wextra -O2)
    # Enable POSIX features (e.g. clock_gettime)
    target_compile_definitions(app PRIVATE _POSIX_C_SOURCE=199309L)
endif()

# --- Unit Testing ---
enable_testing()

# --- Code Coverage ---
option(CODE_COVERAGE "Enable coverage reporting" OFF)
if(CODE_COVERAGE)
    message(STATUS "Code Coverage: ON (Using llvm-cov)")
    add_compile_options(-fprofile-instr-generate -fcoverage-mapping)
    add_link_options(-fprofile-instr-generate)
endif()

# Fetch Unity (Lightweight C Testing Framework)
FetchContent_Declare(
    unity
    GIT_REPOSITORY https://github.com/ThrowTheSwitch/Unity.git
    GIT_TAG        v2.6.0
)
FetchContent_MakeAvailable(unity)

# Add tests subdirectory
add_subdirectory(tests)
