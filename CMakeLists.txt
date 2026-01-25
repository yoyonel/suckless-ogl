cmake_minimum_required(VERSION 3.14)
project(suckless-ogl C)

# Option pour activer le rendu SSBO
option(USE_SSBO_RENDERING "Use SSBO-based rendering instead of instanced rendering" OFF)

if(USE_SSBO_RENDERING)
    add_compile_definitions(USE_SSBO_RENDERING)
    message(STATUS "SSBO rendering: ENABLED")
else()
    message(STATUS "SSBO rendering: DISABLED (using legacy instanced rendering)")
endif()

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
    src/fps.c
    src/ui.c
    src/pbr.c
    src/material.c
    src/perf_timer.c
    src/instanced_rendering.c
    src/ssbo_rendering.c
    src/camera.c
    src/postprocess.c
    src/effects/fx_bloom.c
    src/effects/fx_dof.c
    src/effects/fx_auto_exposure.c
    src/effects/fx_motion_blur.c
    src/window.c
    src/gl_debug.c
)

# Silence warnings about legacy CMake versions in subprojects (e.g., cglm, glad)
set(CMAKE_WARN_DEPRECATED OFF CACHE BOOL "" FORCE)

# Dependencies (automated via FetchContent)
include(FetchContent)

# Force all FetchContent dependencies to build as STATIC libraries
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build static libraries only" FORCE)

# --- Offline Support Logic ---
# If a 'deps' directory exists, we check for subdirectories and use them as source overrides.
# This allows building offline if the dependencies are already cloned there.
set(OFFLINE_DEPS_DIR "${CMAKE_SOURCE_DIR}/deps")
if(EXISTS "${OFFLINE_DEPS_DIR}")
    message(STATUS "Offline mode: Checking for local dependencies in ${OFFLINE_DEPS_DIR}")

    # List of dependencies we want to support overrides for
    set(MANIFEST_DEPS "cglm" "glad" "stb" "unity" "cjson")

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

# 2. GLAD (Automatic Generation - STATIC)
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

# Force GLAD to build as STATIC library
set(GLAD_INSTALL OFF CACHE BOOL "Disable GLAD install" FORCE)

FetchContent_MakeAvailable(glad)

# Ensure glad target is STATIC (override if needed)
if(TARGET glad)
    get_target_property(glad_type glad TYPE)
    if(NOT glad_type STREQUAL "STATIC_LIBRARY")
        message(STATUS "Converting glad to STATIC_LIBRARY")
        set_target_properties(glad PROPERTIES
            POSITION_INDEPENDENT_CODE OFF
        )
    endif()
endif()

# 3. STB (Header-only)
message(STATUS "Fetching stb...")
FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG        master # stb doesn't have regular tags, master is common
)
FetchContent_MakeAvailable(stb)

# 4. GLFW (System Package)
find_package(PkgConfig REQUIRED)
pkg_check_modules(GLFW3 REQUIRED glfw3)

# 5. cJSON (Static Build - CONFIGURATION AMÉLIORÉE)
message(STATUS "Fetching cJSON...")
FetchContent_Declare(
    cjson
    GIT_REPOSITORY https://github.com/DaveGamble/cJSON.git
    GIT_TAG        v1.7.18
)

# Options pour garder le build "suckless" et FORCER le build statique
set(ENABLE_CJSON_TEST OFF CACHE BOOL "Disable cJSON tests" FORCE)
set(ENABLE_CJSON_UTILS OFF CACHE BOOL "Disable cJSON utils" FORCE)
set(ENABLE_TARGET_EXPORT OFF CACHE BOOL "Disable target export" FORCE)
set(BUILD_SHARED_AND_STATIC_LIBS OFF CACHE BOOL "Do not build both shared and static" FORCE)

# Forcer explicitement le build statique de cJSON
set(CJSON_BUILD_SHARED_LIBS OFF CACHE BOOL "Build cJSON as static library" FORCE)

FetchContent_MakeAvailable(cjson)

# --- Configuration du profil Profiling (équivalent Cargo) ---
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Choose the type of build." FORCE)
endif()

# Création d'un profil spécifique "Profiling"
set(CMAKE_C_FLAGS_PROFILING "-O3 -g -fno-omit-frame-pointer" CACHE STRING "Flags for profiling" FORCE)
set(CMAKE_EXE_LINKER_FLAGS_PROFILING "" CACHE STRING "Linker flags for profiling" FORCE)

# Activation du LTO (Link Time Optimization)
include(CheckIPOSupported)
check_ipo_supported(RESULT lto_supported OUTPUT lto_error)

if(lto_supported)
    message(STATUS "LTO (IPO) is supported and enabled for Profiling/Release")
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO ON)
    # On l'active aussi pour notre mode personnalisé
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_PROFILING ON)
endif()

# App target
add_executable(app ${SOURCES})

# Target properties and includes
target_include_directories(app PRIVATE src include ${stb_SOURCE_DIR})
target_include_directories(app PRIVATE ${cjson_SOURCE_DIR})

# Link avec la version statique de cJSON
target_link_libraries(app PRIVATE
    cglm
    glad
    cjson  # Ceci link automatiquement la version statique grâce aux options ci-dessus
    ${GLFW3_LIBRARIES}
    m
    dl
)

target_compile_definitions(app PRIVATE GLFW_INCLUDE_NONE)

# Compiler flags
# Compiler flags
if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(app PRIVATE -Wall -Wextra)

    # -fno-omit-frame-pointer est CRUCIAL pour que les profilers (perf, gprof, callgrind)
    # puissent reconstruire la pile d'appel (stack trace) correctement.
    if(CMAKE_BUILD_TYPE STREQUAL "Profiling" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
        target_compile_options(app PRIVATE -fno-omit-frame-pointer)
    endif()

    add_compile_definitions(_POSIX_C_SOURCE=200809L)
endif()

# Vérification post-build pour s'assurer que tout est statique
add_custom_command(TARGET app POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E echo "Checking for dynamic dependencies..."
    COMMAND ldd $<TARGET_FILE:app> || true
    COMMENT "Binary dependencies check"
)

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
