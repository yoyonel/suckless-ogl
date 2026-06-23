cmake_minimum_required(VERSION 3.14)
project(suckless-ogl C CXX)

# Generate compile_commands.json for generic clang-tidy usage
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# --- Clang-Tidy & Caching ---
option(ENABLE_CLANG_TIDY "Enable clang-tidy linting during build" OFF)

if(ENABLE_CLANG_TIDY)
  find_program(CLANG_TIDY_BIN NAMES clang-tidy)
  if(CLANG_TIDY_BIN)
    set(CLANG_TIDY_CMD "${CLANG_TIDY_BIN};--quiet")
    message(STATUS "Clang-Tidy: ENABLED")
  else()
    message(WARNING "clang-tidy not found, linting disabled")
  endif()
endif()

# Setup ccache if available (standard compiler launcher)
find_program(CCACHE_BIN NAMES ccache)
if(CCACHE_BIN)
  set(CMAKE_C_COMPILER_LAUNCHER "${CCACHE_BIN}")
  set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_BIN}")
  message(STATUS "ccache: ENABLED")
endif()

# Option pour activer le rendu SSBO
option(USE_SSBO_RENDERING "Use SSBO-based rendering instead of instanced rendering" OFF)

# Option pour activer AddressSanitizer
option(ENABLE_ASAN "Enable AddressSanitizer (ASan) for memory checking" OFF)

if(ENABLE_ASAN)
  if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    # AddressSanitizer should only be on for the application and our libs, not external deps
    # so we'll apply it later to the target.
    message(STATUS "AddressSanitizer (ASan): ENABLED (Optimization: OFF)")
  endif()
endif()

if(USE_SSBO_RENDERING)
  add_compile_definitions(USE_SSBO_RENDERING)
  message(STATUS "SSBO rendering: ENABLED")
else()
  message(STATUS "SSBO rendering: DISABLED (using legacy instanced rendering)")
endif()

# Option pour activer le debug synchrone (tueur de performance)
option(DEBUG_SYNCHRONOUS "Enable synchronous OpenGL debug output" OFF)
if(DEBUG_SYNCHRONOUS)
  add_compile_definitions(DEBUG_SYNCHRONOUS)
  message(WARNING "OpenGL Synchronous Debug: ENABLED (Expect low performance)")
endif()

# Option pour activer l'optimisation des shaders
option(ENABLE_SHADER_OPTIMIZATION "Enable optimized shader compilation (static defines)" OFF)
if(ENABLE_SHADER_OPTIMIZATION)
  message(STATUS "Shader Optimization: ENABLED (Static compile)")
endif()

# Option pour activer les tests
option(BUILD_TESTS "Build and enable unit tests" ON)

# Tracy Profiler Integration
option(ENABLE_TRACY "Enable Tracy Profiler integration" OFF)
if(ENABLE_TRACY)
  add_compile_definitions(TRACY_ENABLE TRACY_FIBERS)
  message(STATUS "Tracy Profiler: ENABLED (with Fibers)")
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)



set(SOURCES
    src/action_notifier.c
    src/adaptive_sampler.c
    src/app.c
    src/app_binding.c
    src/asset_manager.c
    src/env_manager.c
    src/app_input.c
    src/app_input_state.c
    src/app_metrics.c
    src/app_profiling.c
    src/app_subsystem.c
    src/app_ui.c
    src/app_window.c
    src/async_loader.c
    src/async_backend.c
    src/async_backend_stb.c
    src/async_backend_ktx.c
    src/async_coordinator.c
    src/billboard_rendering.c
    src/camera.c
    src/camera_input.c
    src/cli.c
    src/effect_benchmark.c
    src/fps.c
    src/lum_histogram.c
    src/gamepad_input.c
    src/gl_debug.c
    src/gpu_profiler.c
    src/gpu_profiler_ui.c
    src/gpu_usage.c
    src/ibl_coordinator.c
    src/icosphere.c
    src/instanced_rendering.c
    src/light_probes.c
    src/io.c
    src/log.c
    src/main.c
    src/material.c
    src/metric_stack.c
    src/nbody/nbody_physics.c
    src/nbody/nbody_preset.c
    src/nbody/nbody_diagnostics.c
    src/nbody/nbody_render.c
    src/pbr.c
    src/perf_mode.c
    src/perf_timer.c
    src/postprocess_apply.c
    src/postprocess_cleanup.c
    src/postprocess_init.c
    src/postprocess_input.c
    src/postprocess_presets.c
    src/postprocess_readback.c
    src/postprocess_setters.c
    src/postprocess_shader.c
    src/render_utils.c
    src/scene_cleanup.c
    src/scene_init.c
    src/scene_nbody.c
    src/scene_render.c
    src/shader.c
    src/sh_math.c
    src/simd_utils.c
    src/skybox.c
    src/billboard_sorting.c
    src/ssbo_rendering.c
    src/stb_image_impl.c
    src/texture.c
    src/tracy_gpu.cpp
    src/trail_renderer.c
    src/shockwave.c
    src/tracy_log.c
    src/tracy_manager.c
    src/ui.c
    src/utils.c
    src/window.c
    src/renderer.c
    src/effects/fx_auto_exposure.c
    src/effects/fx_bloom.c
    src/effects/fx_dof.c
    src/effects/fx_lut3d.c
    src/effects/fx_lut_viz.c
    src/effects/fx_motion_blur.c
    src/effects/fx_utils.c
    src/platform/platform_fs.c
    src/platform/platform_time.c
    src/platform/platform_utils.c
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
  set(MANIFEST_DEPS "cglm" "glad" "stb" "unity" "cjson" "glfw" "libktx")

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

# 4. GLFW (Force FetchContent for Host IDE Support)
# We deliberately skip pkg-config to ensure headers are available in build/_deps
# This allows clangd on the host to index them even if they are not installed on the host system.
message(STATUS "Forcing GLFW3 FetchContent for IDE compatibility...")
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.3.8
)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)
set(GLFW3_LIBRARIES glfw)

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

# Fix warnings for external dependencies when cross-compiling or using strict flags
if(TARGET cjson)
  if(CMAKE_CROSSCOMPILING OR (CMAKE_C_COMPILER_ID MATCHES "GNU" AND WIN32))
    target_compile_options(cjson PRIVATE -Wno-float-conversion -Wno-error)
  endif()
endif()
if(TARGET cglm)
  # cglm is usually warning-clean, but good to be safe
  target_compile_options(cglm PRIVATE -Wno-error)
endif()

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

if(lto_supported AND NOT ENABLE_ASAN)
  message(STATUS "LTO (IPO) is supported and enabled for Profiling/Release")
  set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
  set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO ON)
  # On l'active aussi pour notre mode personnalisé
  set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_PROFILING ON)
elseif(ENABLE_ASAN)
  message(STATUS "LTO (IPO) is DISABLED because AddressSanitizer is active")
  set(CMAKE_INTERPROCEDURAL_OPTIMIZATION OFF)
  set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE OFF)
  set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO OFF)
  set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_PROFILING OFF)
endif()

# Mettre le transcodeur KTX au régime sec sur les formats mobiles
# (On conserve uniquement les chemins de code utiles pour OpenGL Desktop : BC7, DXT1, etc.)
add_compile_definitions(
    BASISD_SUPPORT_ETC2_EAC_A8=0
    BASISD_SUPPORT_ASTC=0
    BASISD_SUPPORT_PVRTC1=0
    BASISD_SUPPORT_ATC=0
    BASISD_SUPPORT_FXT1=0
    BASISD_SUPPORT_PVRTC2=0
)

# 6. libktx (Static Build)
message(STATUS "Fetching libktx...")
include(FetchContent)
FetchContent_Declare(
    libktx
    GIT_REPOSITORY https://github.com/KhronosGroup/KTX-Software.git
    GIT_TAG        v5.0.0-rc1
)
# --- libktx : Désactivation stricte des encodeurs ---
set(KTX_FEATURE_TESTS OFF CACHE BOOL "Disable KTX tests" FORCE)
set(KTX_FEATURE_TOOLS OFF CACHE BOOL "Disable KTX tools" FORCE)
set(KTX_FEATURE_WRITE OFF CACHE BOOL "Disable KTX write/encode support" FORCE)
set(KTX_FEATURE_GL_UPLOAD ON CACHE BOOL "Enable KTX GL upload" FORCE)
set(KTX_FEATURE_STATIC_LIBRARY ON CACHE BOOL "" FORCE)

# FORCER BASIS UNIVERSAL À NE COMPILER QUE LE TRANSCODEUR
set(BASISU_SUPPORT_ENCODING OFF CACHE BOOL "Disable Basis encoder" FORCE)
set(BASISU_SUPPORT_UASTC OFF CACHE BOOL "Disable Basis UASTC encoder" FORCE)

FetchContent_MakeAvailable(libktx)

# App target
set(APP_SOURCES ${SOURCES})
if(ENABLE_TRACY)
  list(APPEND APP_SOURCES "deps/tracy/public/TracyClient.cpp")
  set_source_files_properties("deps/tracy/public/TracyClient.cpp" PROPERTIES COMPILE_FLAGS "-w")
endif()
add_executable(app ${APP_SOURCES})

# --- Optimisations Avancées ---
option(ENABLE_NATIVE_ARCH "Enable native architecture optimizations (-march=native)" ON)
option(ENABLE_AGGRESSIVE_MATH "Enable aggressive math optimizations (-ffast-math)" OFF)
option(ENABLE_UNITY_BUILD "Enable Unity/Jumbo Build (single compilation unit)" OFF)

if(ENABLE_UNITY_BUILD)
  set(CMAKE_UNITY_BUILD ON)
  message(STATUS "Unity Build: ENABLED")
endif()

if(ENABLE_NATIVE_ARCH AND NOT CMAKE_CROSSCOMPILING)
  if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(app PRIVATE -march=native -mavx2 -mf16c)
    message(STATUS "Native Architecture Optimization: ENABLED (Flags: -march=native -mavx2 -mf16c)")
  else()
    message(WARNING "Native Architecture Optimization enabled but compiler '${CMAKE_C_COMPILER_ID}' not recognized")
  endif()
elseif(ENABLE_NATIVE_ARCH AND CMAKE_CROSSCOMPILING)
  message(STATUS "Native Architecture Optimization: DISABLED (Cross-compiling)")
endif()

if(ENABLE_AGGRESSIVE_MATH)
  if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(app PRIVATE -ffast-math)
    message(STATUS "Aggressive Math Optimization: ENABLED")
  endif()
endif()

# Force standard floating point behavior for the N-body physics core
# even if aggressive math is enabled for the rest of the application.
if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
  set_source_files_properties(
        src/nbody/nbody_physics.c
        src/nbody/nbody_diagnostics.c
        src/nbody/nbody_preset.c
        PROPERTIES COMPILE_FLAGS "-fno-fast-math"
    )
endif()

# Apply Clang-Tidy only to this target
if(ENABLE_CLANG_TIDY AND CLANG_TIDY_CMD)
  set_target_properties(app PROPERTIES C_CLANG_TIDY "${CLANG_TIDY_CMD}")
endif()

# Target properties and includes
target_include_directories(app PRIVATE
    src
    src/nbody
    include
    ${stb_SOURCE_DIR}
    ${cjson_SOURCE_DIR}
    ${glfw_SOURCE_DIR}/include
    ${glad_BINARY_DIR}/include
    ${cglm_SOURCE_DIR}/include
)
if(ENABLE_TRACY)
  target_include_directories(app PUBLIC deps/tracy/public)
endif()

# Link avec la version statique de cJSON
find_package(Threads REQUIRED)
# Link libraries
target_link_libraries(app PRIVATE
    cglm
    glad
    cjson
    ${GLFW3_LIBRARIES}
    Threads::Threads
    ktx_read
)

if(UNIX)
  target_link_libraries(app PRIVATE m dl)
endif()

if(WIN32)
  # Windows-specific libraries can be added here
  # Use static linking for runtime to avoid missing libgcc/libwinpthread DLLs
  target_link_options(app PRIVATE -static)
endif()

# Optional GameMode integration for performance optimization
# Requires both the library AND the header to be installed
find_library(GAMEMODE_LIB NAMES gamemode)
find_path(GAMEMODE_INCLUDE_DIR NAMES gamemode_client.h)
if(GAMEMODE_LIB AND GAMEMODE_INCLUDE_DIR)
  message(STATUS "GameMode found: ${GAMEMODE_LIB}")
  message(STATUS "GameMode headers: ${GAMEMODE_INCLUDE_DIR}")
  target_compile_definitions(app PRIVATE HAVE_GAMEMODE)
  target_include_directories(app PRIVATE ${GAMEMODE_INCLUDE_DIR})
  target_link_libraries(app PRIVATE ${GAMEMODE_LIB})
else()
  if(GAMEMODE_LIB AND NOT GAMEMODE_INCLUDE_DIR)
    message(STATUS "GameMode library found but headers missing - install libgamemode-dev")
  endif()
  message(STATUS "GameMode not available - using native scheduling fallback")
endif()

target_compile_definitions(app PRIVATE GLFW_INCLUDE_NONE)

if(ENABLE_ASAN)
  if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(app PRIVATE -fsanitize=address -fno-omit-frame-pointer)
    target_link_options(app PRIVATE -fsanitize=address)
    # Force -O0 for ASan to ensure manual leaks are not optimized away
    set(CMAKE_C_FLAGS_DEBUG "-O0 -g" CACHE STRING "Flags for Debug" FORCE)
  endif()
endif()

# Définir une option que vous pouvez activer lors de la configuration
option(ENABLE_TSAN "Activer ThreadSanitizer pour l'analyse de concurrence" OFF)

if(ENABLE_TSAN)
  message(STATUS "ThreadSanitizer est ACTIVÉ")
  # Ajout des flags de compilation (-g est crucial pour les numéros de ligne)
  target_compile_options(app PRIVATE -fsanitize=thread -g -O1)
  # Ajout des flags pour l'éditeur de liens (linker)
  target_link_options(app PRIVATE -fsanitize=thread)
  # (Optionnel) Si vous utilisez GCC/Clang, forcer la position independent code
  set_target_properties(app PROPERTIES POSITION_INDEPENDENT_CODE TRUE)
endif()

if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
  # _FORTIFY_SOURCE=2: Enables compile-time and runtime checks for buffer overflows
  # (Requires optimization level -O1 or higher)
  # We only enable it for non-Debug builds to avoid "#warning _FORTIFY_SOURCE requires compiling with optimization"
  if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(app PRIVATE _FORTIFY_SOURCE=2)
  endif()

  # Linker Hardening:
  # -Wl,-z,relro: Read-only Relocations (protects GOT)
  # -Wl,-z,now: Immediate Binding (protects PLT)
  # -Wl,-z,noexecstack: Prevents code execution on the stack
  if(UNIX AND NOT APPLE)
    target_link_options(app PRIVATE
            -Wl,-z,relro
            -Wl,-z,now
            -Wl,-z,noexecstack
        )
  endif()

  # -fno-omit-frame-pointer est CRUCIAL pour que les profilers (perf, gprof, callgrind)
  # puissent reconstruire la pile d'appel (stack trace) correctement.
  if((CMAKE_BUILD_TYPE STREQUAL "Profiling" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo") AND (CMAKE_C_COMPILER_ID MATCHES "GNU" OR CMAKE_C_COMPILER_ID MATCHES "Clang"))
    target_compile_options(app PRIVATE -fno-omit-frame-pointer)
  endif()

  target_compile_definitions(app PRIVATE _GNU_SOURCE _POSIX_C_SOURCE=200809L)
endif()

if(ENABLE_SHADER_OPTIMIZATION)
  target_compile_definitions(app PRIVATE ENABLE_SHADER_OPTIMIZATION)
endif()

# Vérification post-build pour s'assurer que tout est statique
if(UNIX AND NOT APPLE)
  add_custom_command(TARGET app POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E echo "Checking for dynamic dependencies..."
        COMMAND ldd $<TARGET_FILE:app> || true
        COMMENT "Binary dependencies check"
    )
endif()

# --- Unit Testing ---
if(BUILD_TESTS AND NOT ENABLE_TRACY)
  enable_testing()

  # --- Code Coverage ---
  option(CODE_COVERAGE "Enable coverage reporting" OFF)
  if(CODE_COVERAGE)
    if(CMAKE_C_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
      message(STATUS "Code Coverage: ON (Using llvm-cov flags)")
      add_compile_options(-fprofile-instr-generate -fcoverage-mapping)
      add_link_options(-fprofile-instr-generate)
    elseif(CMAKE_C_COMPILER_ID MATCHES "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "GNU")
      message(STATUS "Code Coverage: ON (Using gcov flags)")
      add_compile_options(--coverage)
      add_link_options(--coverage)
    endif()
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
else()
  message(STATUS "Unit Testing: DISABLED (Tracy Profiler Active)")
endif()

# Target pour nettoyer uniquement le dossier d'analyse
add_custom_target(audit-clean
    COMMAND ${CMAKE_COMMAND} -E remove_directory "${CMAKE_BINARY_DIR}/build-audit"
    COMMENT "Nettoyage des artefacts d'audit"
)

# Audit simple : on suppose que build-audit est déjà configuré et compilé
add_custom_target(audit
    COMMAND bloaty "${CMAKE_BINARY_DIR}/app" -d compileunits -n 20
    COMMENT "Audit de binaire complet"
)

# TODO: à intégrer mais faut réflêchir ^^
# add_custom_command(TARGET audit POST_BUILD
#     COMMAND sh -c "if [ $(du -k ${CMAKE_BINARY_DIR}/build-audit/app | cut -f1) -gt 3000 ]; then echo 'ERREUR: Binaire trop gros !'; exit 1; fi"
# )

# --- KTX Viewer Standalone Target ---
# On s'assure que le fichier source existe avant d'ajouter la cible
if(EXISTS "${CMAKE_SOURCE_DIR}/src/ktx_viewer.c")
  add_executable(ktx_viewer src/ktx_viewer.c src/utils.c)

  # On inclut les headers générés par FetchContent
  target_include_directories(ktx_viewer PRIVATE
        include
        ${glfw_SOURCE_DIR}/include
        ${glad_BINARY_DIR}/include
    )

  # On link avec les mêmes libs statiques que l'app principale
  target_link_libraries(ktx_viewer PRIVATE
        glad
        ${GLFW3_LIBRARIES}
        ktx
        Threads::Threads
    )

  if(UNIX)
    target_link_libraries(ktx_viewer PRIVATE m dl)
  endif()

  if(WIN32)
    target_link_options(ktx_viewer PRIVATE -static)
  endif()

  if(ENABLE_CLANG_TIDY AND CLANG_TIDY_CMD)
    set_target_properties(ktx_viewer PROPERTIES C_CLANG_TIDY "${CLANG_TIDY_CMD}")
  endif()

  message(STATUS "Added target: ktx_viewer")
endif()
