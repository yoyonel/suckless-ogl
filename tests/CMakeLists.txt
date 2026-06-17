# tests/CMakeLists.txt - Version avec auto-découverte des tests
# (Touched to force re-glob)

# =============================================================================
# CONFIGURATION COMMUNE
# =============================================================================

set(TEST_COMMON_LIBS unity cglm glad cjson ${GLFW3_LIBRARIES} Threads::Threads ktx)
if(UNIX)
  list(APPEND TEST_COMMON_LIBS m dl)
endif()
set(TEST_INCLUDE_DIRS
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
    ${stb_SOURCE_DIR}
    ${cjson_SOURCE_DIR}
    ${glfw_SOURCE_DIR}/include
    ${glad_BINARY_DIR}/include
    ${cglm_SOURCE_DIR}/include
)

if(ENABLE_TRACY)
  list(APPEND TEST_INCLUDE_DIRS "${CMAKE_SOURCE_DIR}/deps/tracy/public")
endif()

# =============================================================================
# BIBLIOTHÈQUE PARTAGÉE POUR LES TESTS
# =============================================================================

# Convertir les chemins relatifs de SOURCES en chemins absolus
set(TEST_LIB_SOURCES "")
foreach(source ${SOURCES})
  # Si le chemin est déjà absolu, le garder tel quel
  if(IS_ABSOLUTE ${source})
    list(APPEND TEST_LIB_SOURCES ${source})
  else()
    # Sinon, le préfixer avec CMAKE_SOURCE_DIR
    list(APPEND TEST_LIB_SOURCES ${CMAKE_SOURCE_DIR}/${source})
  endif()
endforeach()

# Exclure main.c
list(FILTER TEST_LIB_SOURCES EXCLUDE REGEX ".*main\\.c$")

# Créer la bibliothèque de test (compilée une seule fois)
if(ENABLE_TRACY)
  list(APPEND TEST_LIB_SOURCES "${CMAKE_SOURCE_DIR}/deps/tracy/public/TracyClient.cpp")
endif()

# Force Unity to support doubles
if(TARGET unity)
  target_compile_definitions(unity PUBLIC UNITY_INCLUDE_DOUBLE)
endif()

add_library(app_testlib STATIC ${TEST_LIB_SOURCES})
target_include_directories(app_testlib PUBLIC ${TEST_INCLUDE_DIRS})
target_link_libraries(app_testlib PUBLIC ${TEST_COMMON_LIBS})
target_compile_definitions(app_testlib PUBLIC
    UNITY_INCLUDE_DOUBLE
    _GNU_SOURCE
    _DEFAULT_SOURCE
    _POSIX_C_SOURCE=200809L
)

# =============================================================================
# AUTO-DÉCOUVERTE DES TESTS
# =============================================================================

# Lister tous les fichiers test_*.c dans le répertoire tests/
file(GLOB TEST_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/test_*.c")

# Exclure les tests standalone de la boucle générique
list(FILTER TEST_SOURCES EXCLUDE REGEX ".*test_shader_path_security_standalone\\.c$")
list(FILTER TEST_SOURCES EXCLUDE REGEX ".*test_billboard_cleanup\\.c$")
list(FILTER TEST_SOURCES EXCLUDE REGEX ".*test_texture_toctou\\.c$")
list(FILTER TEST_SOURCES EXCLUDE REGEX ".*test_billboard_overflow\\.c$")
list(FILTER TEST_SOURCES EXCLUDE REGEX ".*test_gpu_profiler_repro\\.c$")
list(FILTER TEST_SOURCES EXCLUDE REGEX ".*test_shader_limit\\.c$")
list(FILTER TEST_SOURCES EXCLUDE REGEX ".*test_camera_input\\.c$")
list(FILTER TEST_SOURCES EXCLUDE REGEX ".*test_gamepad_input\\.c$")
list(FILTER TEST_SOURCES EXCLUDE REGEX ".*test_ibl_coordinator\\.c$")
list(FILTER TEST_SOURCES EXCLUDE REGEX ".*test_benchmark_billboard_sorting\\.c$")
list(FILTER TEST_SOURCES EXCLUDE REGEX ".*test_billboard_sorting\\.c$")
list(FILTER TEST_SOURCES EXCLUDE REGEX ".*test_benchmark_billboard_sorting_perf\\.c$")
list(FILTER TEST_SOURCES EXCLUDE REGEX ".*test_sh_precision\\.c$")
list(FILTER TEST_SOURCES EXCLUDE REGEX ".*test_sh_integration\\.c$")
list(FILTER TEST_SOURCES EXCLUDE REGEX ".*test_async_coordinator\\.c$")
list(FILTER TEST_SOURCES EXCLUDE REGEX ".*test_async_loader\\.c$")
list(FILTER TEST_SOURCES EXCLUDE REGEX ".*test_texture_security\\.c$")
list(FILTER TEST_SOURCES EXCLUDE REGEX ".*test_ibl_extract\\.c$")
list(FILTER TEST_SOURCES EXCLUDE REGEX ".*test_scene_nbody\\.c$")
list(FILTER TEST_SOURCES EXCLUDE REGEX ".*test_shockwave\\.c$")
list(FILTER TEST_SOURCES EXCLUDE REGEX ".*test_trail_renderer\\.c$")
list(FILTER TEST_SOURCES EXCLUDE REGEX ".*test_postprocess_input\\.c$")
list(FILTER TEST_SOURCES EXCLUDE REGEX ".*test_scene_render\\.c$")

# Exclure les tests spécifiques à POSIX sur Windows
if(WIN32)
  message(STATUS "Excluding POSIX-only tests from Windows build")
  list(FILTER TEST_SOURCES EXCLUDE REGEX ".*test_log\\.c$")
  list(FILTER TEST_SOURCES EXCLUDE REGEX ".*test_async_loader_perf\\.c$")
  list(FILTER TEST_SOURCES EXCLUDE REGEX ".*test_shader\\.c$")
  list(FILTER TEST_SOURCES EXCLUDE REGEX ".*test_shader_api\\.c$")
  list(FILTER TEST_SOURCES EXCLUDE REGEX ".*test_shader_uniforms_loc\\.c$")
  list(FILTER TEST_SOURCES EXCLUDE REGEX ".*test_gpu_usage\\.c$")
endif()

# Tests qui nécessitent OpenGL (à mettre dans Xvfb)
set(OPENGL_TESTS
    test_shader
    test_shader_api
    test_texture
    test_skybox
    test_pbr
    test_material
    test_ui
    test_instanced_rendering
    test_ssbo_rendering
    test_app
    test_postprocess
    test_fxaa_synthetic
    test_render_utils
    test_benchmark_histogram
    test_gpu_profiler
    test_app_input
    test_app_metrics
    test_app_subsystem
    test_billboard_perf
    test_shader_uniforms_loc
    test_stencil_masking
    test_env_transition
    test_auto_exposure_debug
    test_fx_lut3d
    test_fx_dof
)


# Pour chaque fichier test trouvé
foreach(test_file ${TEST_SOURCES})
  # Extraire le nom du test (sans .c)
  get_filename_component(test_name ${test_file} NAME_WE)

  # Créer l'exécutable
  add_executable(${test_name} ${test_file})

  # Condition spécifique pour le linkage de KTX
  if(${test_name} STREQUAL "test_ktx_link")
    target_link_libraries(${test_name} PRIVATE unity ktx)
  else()
    target_link_libraries(${test_name} PRIVATE app_testlib)
  endif()

  # Injecter les variables pour le test de sécurité du log
  if(${test_name} STREQUAL "test_log_security")
    target_compile_definitions(${test_name} PRIVATE
            TEST_COMPILER=\"${CMAKE_C_COMPILER}\"
            TEST_INCLUDE_DIR=\"${CMAKE_SOURCE_DIR}/include\"
        )
  endif()

  # Déterminer si c'est un test OpenGL ou si on est sur Windows (cross-compilation)
  if(${test_name} IN_LIST OPENGL_TESTS OR WIN32)
    # Test avec Xvfb (et Wine sur Windows via le wrapper)
    add_test(NAME ${test_name}
                 COMMAND ${CMAKE_SOURCE_DIR}/.github/workflows/scripts/run_test_with_xvfb.sh $<TARGET_FILE:${test_name}>)
    message(STATUS "Test découvert (avec wrapper): ${test_name}")
  else()
    # Test simple
    add_test(NAME ${test_name} COMMAND ${test_name})
    message(STATUS "Test unitaire découvert: ${test_name}")
  endif()

  # Working directory = racine du projet
  set_tests_properties(${test_name} PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

  # Serialize OpenGL tests to prevent GPU/display contention when running
  # with ctest -j$(nproc). Non-GPU tests remain fully parallelizable.
  if(${test_name} IN_LIST OPENGL_TESTS)
    set_tests_properties(${test_name} PROPERTIES RESOURCE_LOCK "gpu")
  endif()
endforeach()

# =============================================================================
# TEST STANDALONE (MOCKED)
# =============================================================================
# Ce test a besoin de ses propres mocks pour GLAD et GLFW, et ne doit PAS
# être lié à app_testlib ou glad (pour éviter les conflits de symboles).

# =============================================================================
# TEST STANDALONE (MOCKED)
# =============================================================================
# Ce test a besoin de ses propres mocks pour GLAD et GLFW, et ne doit PAS
# être lié à app_testlib ou glad (pour éviter les conflits de symboles).

add_executable(test_shader_path_security_standalone
    test_shader_path_security_standalone.c
    ${unity_SOURCE_DIR}/src/unity.c
    ${CMAKE_SOURCE_DIR}/tests/mocks/standalone/mock_gl_standalone.c
    ${CMAKE_SOURCE_DIR}/src/io.c
    ${CMAKE_SOURCE_DIR}/src/utils.c
)
if(ENABLE_TRACY)
  target_sources(test_shader_path_security_standalone PRIVATE "${CMAKE_SOURCE_DIR}/deps/tracy/public/TracyClient.cpp")
endif()

target_include_directories(test_shader_path_security_standalone PRIVATE
    ${CMAKE_SOURCE_DIR}/tests/mocks
    ${CMAKE_SOURCE_DIR}/tests/mocks/standalone
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
    ${stb_SOURCE_DIR}
    ${unity_SOURCE_DIR}/src
)
if(ENABLE_TRACY)
  target_include_directories(test_shader_path_security_standalone PRIVATE "${CMAKE_SOURCE_DIR}/deps/tracy/public")
endif()

# On ne link PAS contre app_testlib, seulement libc/libm si besoin
# (Mais on a besoin des headers de cglm pour shader.c s'il est inclus ?)
# shader.c n'utilise pas cglm directement, mais inclut shader.h qui peut l'utiliser via app_settings.h?
# Vérifions : shader.h -> app_settings.h -> cglm/types.h
# Donc on a besoin de cglm.

# Si cglm est header-only ou statique:
target_link_libraries(test_shader_path_security_standalone PRIVATE cglm m)
target_compile_definitions(test_shader_path_security_standalone PRIVATE _GNU_SOURCE)

if(WIN32)
  add_test(NAME test_shader_path_security_standalone
             COMMAND ${CMAKE_SOURCE_DIR}/.github/workflows/scripts/run_test_with_xvfb.sh $<TARGET_FILE:test_shader_path_security_standalone>)
else()
  add_test(NAME test_shader_path_security_standalone
             COMMAND test_shader_path_security_standalone)
endif()

add_executable(test_texture_toctou
    test_texture_toctou.c
    ${unity_SOURCE_DIR}/src/unity.c
    ${CMAKE_SOURCE_DIR}/tests/mocks/standalone/mock_gl_standalone.c
    ${CMAKE_SOURCE_DIR}/tests/mocks/standalone/mock_stb_image_standalone.c
    ${CMAKE_SOURCE_DIR}/src/simd_utils.c
    ${CMAKE_SOURCE_DIR}/src/io.c
    ${CMAKE_SOURCE_DIR}/src/utils.c
)
if(ENABLE_TRACY)
  target_sources(test_texture_toctou PRIVATE "${CMAKE_SOURCE_DIR}/deps/tracy/public/TracyClient.cpp")
endif()
target_include_directories(test_texture_toctou PRIVATE
    ${CMAKE_SOURCE_DIR}/tests/mocks
    ${CMAKE_SOURCE_DIR}/tests/mocks/standalone
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
    ${stb_SOURCE_DIR}
    ${unity_SOURCE_DIR}/src
)
if(ENABLE_TRACY)
  target_include_directories(test_texture_toctou PRIVATE "${CMAKE_SOURCE_DIR}/deps/tracy/public")
endif()
target_link_libraries(test_texture_toctou PRIVATE cglm m)
target_compile_definitions(test_texture_toctou PRIVATE _GNU_SOURCE)
if(WIN32)
  add_test(NAME test_texture_toctou
             COMMAND ${CMAKE_SOURCE_DIR}/.github/workflows/scripts/run_test_with_xvfb.sh $<TARGET_FILE:test_texture_toctou>)
else()
  add_test(NAME test_texture_toctou
             COMMAND test_texture_toctou)
endif()

# =============================================================================
# TEST BILLBOARD CLEANUP (MOCKED)
# =============================================================================
add_executable(test_billboard_cleanup
    test_billboard_cleanup.c
    ${unity_SOURCE_DIR}/src/unity.c
    ${CMAKE_SOURCE_DIR}/tests/mocks/standalone/mock_gl_standalone.c
    ${CMAKE_SOURCE_DIR}/src/utils.c
)
if(ENABLE_TRACY)
  target_sources(test_billboard_cleanup PRIVATE "${CMAKE_SOURCE_DIR}/deps/tracy/public/TracyClient.cpp")
endif()

target_include_directories(test_billboard_cleanup PRIVATE
    ${CMAKE_SOURCE_DIR}/tests/mocks
    ${CMAKE_SOURCE_DIR}/tests/mocks/standalone
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
    ${stb_SOURCE_DIR}
    ${unity_SOURCE_DIR}/src
)
if(ENABLE_TRACY)
  target_include_directories(test_billboard_cleanup PRIVATE "${CMAKE_SOURCE_DIR}/deps/tracy/public")
endif()

target_link_libraries(test_billboard_cleanup PRIVATE cglm m)

if(WIN32)
  add_test(NAME test_billboard_cleanup
             COMMAND ${CMAKE_SOURCE_DIR}/.github/workflows/scripts/run_test_with_xvfb.sh $<TARGET_FILE:test_billboard_cleanup>)
else()
  add_test(NAME test_billboard_cleanup
             COMMAND test_billboard_cleanup)
endif()

# =============================================================================
# TEST BILLBOARD OVERFLOW (MOCKED)
# =============================================================================
add_executable(test_billboard_overflow
    test_billboard_overflow.c
    ${unity_SOURCE_DIR}/src/unity.c
    ${CMAKE_SOURCE_DIR}/tests/mocks/standalone/mock_gl_standalone.c
    ${CMAKE_SOURCE_DIR}/src/utils.c
)
if(ENABLE_TRACY)
  target_sources(test_billboard_overflow PRIVATE "${CMAKE_SOURCE_DIR}/deps/tracy/public/TracyClient.cpp")
endif()

target_include_directories(test_billboard_overflow PRIVATE
    ${CMAKE_SOURCE_DIR}/tests/mocks
    ${CMAKE_SOURCE_DIR}/tests/mocks/standalone
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
    ${stb_SOURCE_DIR}
    ${unity_SOURCE_DIR}/src
)
if(ENABLE_TRACY)
  target_include_directories(test_billboard_overflow PRIVATE "${CMAKE_SOURCE_DIR}/deps/tracy/public")
endif()

target_link_libraries(test_billboard_overflow PRIVATE cglm)

if(WIN32)
  add_test(NAME test_billboard_overflow
             COMMAND ${CMAKE_SOURCE_DIR}/.github/workflows/scripts/run_test_with_xvfb.sh $<TARGET_FILE:test_billboard_overflow>)
else()
  add_test(NAME test_billboard_overflow
             COMMAND test_billboard_overflow)
endif()

# =============================================================================
# TEST GPU PROFILER REPRO (MOCKED)
# =============================================================================
add_executable(test_gpu_profiler_repro
    test_gpu_profiler_repro.c
    ${unity_SOURCE_DIR}/src/unity.c
    ${CMAKE_SOURCE_DIR}/tests/mocks/standalone/mock_gl_standalone.c
    ${CMAKE_SOURCE_DIR}/src/gpu_profiler.c
    ${CMAKE_SOURCE_DIR}/src/adaptive_sampler.c
    ${CMAKE_SOURCE_DIR}/src/log.c
    ${CMAKE_SOURCE_DIR}/src/tracy_log.c
    ${CMAKE_SOURCE_DIR}/src/metric_stack.c
    ${CMAKE_SOURCE_DIR}/src/utils.c
    ${CMAKE_SOURCE_DIR}/src/platform/platform_utils.c
    ${CMAKE_SOURCE_DIR}/src/platform/platform_time.c
)
if(ENABLE_TRACY)
  target_sources(test_gpu_profiler_repro PRIVATE "${CMAKE_SOURCE_DIR}/deps/tracy/public/TracyClient.cpp")
endif()

target_include_directories(test_gpu_profiler_repro PRIVATE
    ${CMAKE_SOURCE_DIR}/tests/mocks
    ${CMAKE_SOURCE_DIR}/tests/mocks/standalone
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
    ${stb_SOURCE_DIR}
    ${unity_SOURCE_DIR}/src
)
if(ENABLE_TRACY)
  target_include_directories(test_gpu_profiler_repro PRIVATE "${CMAKE_SOURCE_DIR}/deps/tracy/public")
endif()

target_link_libraries(test_gpu_profiler_repro PRIVATE cglm m)

if(WIN32)
  add_test(NAME test_gpu_profiler_repro
             COMMAND ${CMAKE_SOURCE_DIR}/.github/workflows/scripts/run_test_with_xvfb.sh $<TARGET_FILE:test_gpu_profiler_repro>)
else()
  add_test(NAME test_gpu_profiler_repro
             COMMAND test_gpu_profiler_repro)
endif()

# =============================================================================
# TEST SHADER LIMIT (MOCKED)
# =============================================================================
add_executable(test_shader_limit
    test_shader_limit.c
    ${unity_SOURCE_DIR}/src/unity.c
    ${CMAKE_SOURCE_DIR}/tests/mocks/standalone/mock_gl_standalone.c
    ${CMAKE_SOURCE_DIR}/src/io.c
    ${CMAKE_SOURCE_DIR}/src/utils.c
)

target_include_directories(test_shader_limit PRIVATE
    ${CMAKE_SOURCE_DIR}/tests/mocks
    ${CMAKE_SOURCE_DIR}/tests/mocks/standalone
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
    ${stb_SOURCE_DIR}
    ${unity_SOURCE_DIR}/src
)

target_link_libraries(test_shader_limit PRIVATE cglm m)

if(WIN32)
  add_test(NAME test_shader_limit
             COMMAND ${CMAKE_SOURCE_DIR}/.github/workflows/scripts/run_test_with_xvfb.sh $<TARGET_FILE:test_shader_limit>)
else()
  add_test(NAME test_shader_limit
             COMMAND test_shader_limit)
endif()

# =============================================================================
# TEST CAMERA INPUT (ISOLATED)
# =============================================================================
add_executable(test_camera_input
    test_camera_input.c
    ${unity_SOURCE_DIR}/src/unity.c
    ${CMAKE_SOURCE_DIR}/src/camera.c
    ${CMAKE_SOURCE_DIR}/src/camera_input.c
    ${CMAKE_SOURCE_DIR}/src/utils.c
)

target_include_directories(test_camera_input PRIVATE
    ${CMAKE_SOURCE_DIR}/tests/mocks
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
    ${unity_SOURCE_DIR}/src
)

target_link_libraries(test_camera_input PRIVATE cglm m)

if(WIN32)
  add_test(NAME test_camera_input
             COMMAND ${CMAKE_SOURCE_DIR}/.github/workflows/scripts/run_test_with_xvfb.sh $<TARGET_FILE:test_camera_input>)
else()
  add_test(NAME test_camera_input
             COMMAND test_camera_input)
endif()

# =============================================================================
# TEST GAMEPAD INPUT (ISOLATED / MOCKED GLFW)
# =============================================================================
add_executable(test_gamepad_input
    test_gamepad_input.c
    ${unity_SOURCE_DIR}/src/unity.c
    ${CMAKE_SOURCE_DIR}/src/camera.c
    ${CMAKE_SOURCE_DIR}/src/camera_input.c
    ${CMAKE_SOURCE_DIR}/src/gamepad_input.c
    ${CMAKE_SOURCE_DIR}/src/utils.c
)

target_include_directories(test_gamepad_input PRIVATE
    ${CMAKE_SOURCE_DIR}/tests/mocks
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
    ${unity_SOURCE_DIR}/src
)

target_link_libraries(test_gamepad_input PRIVATE cglm m)

if(WIN32)
  add_test(NAME test_gamepad_input
             COMMAND ${CMAKE_SOURCE_DIR}/.github/workflows/scripts/run_test_with_xvfb.sh $<TARGET_FILE:test_gamepad_input>)
else()
  add_test(NAME test_gamepad_input COMMAND test_gamepad_input)
endif()

# =============================================================================
# TEST IBL COORDINATOR (MOCKED)
# =============================================================================
add_executable(test_ibl_coordinator
    test_ibl_coordinator.c
    ${unity_SOURCE_DIR}/src/unity.c
    ${CMAKE_SOURCE_DIR}/tests/mocks/standalone/mock_gl_standalone.c
    ${CMAKE_SOURCE_DIR}/src/ibl_coordinator.c
    ${CMAKE_SOURCE_DIR}/src/log.c
    ${CMAKE_SOURCE_DIR}/src/tracy_log.c
    ${CMAKE_SOURCE_DIR}/src/utils.c
    ${CMAKE_SOURCE_DIR}/src/platform/platform_utils.c
    ${CMAKE_SOURCE_DIR}/src/platform/platform_time.c
)
if(ENABLE_TRACY)
  target_sources(test_ibl_coordinator PRIVATE "${CMAKE_SOURCE_DIR}/deps/tracy/public/TracyClient.cpp")
endif()

target_include_directories(test_ibl_coordinator PRIVATE
    ${CMAKE_SOURCE_DIR}/tests/mocks
    ${CMAKE_SOURCE_DIR}/tests/mocks/standalone
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
    ${stb_SOURCE_DIR}
    ${unity_SOURCE_DIR}/src
)
if(ENABLE_TRACY)
  target_include_directories(test_ibl_coordinator PRIVATE "${CMAKE_SOURCE_DIR}/deps/tracy/public")
endif()

target_link_libraries(test_ibl_coordinator PRIVATE cglm m)

if(WIN32)
  add_test(NAME test_ibl_coordinator
             COMMAND ${CMAKE_SOURCE_DIR}/.github/workflows/scripts/run_test_with_xvfb.sh $<TARGET_FILE:test_ibl_coordinator>)
else()
  add_test(NAME test_ibl_coordinator
             COMMAND test_ibl_coordinator)
endif()

# TEST BENCHMARK SPHERE SORTING (MOCKED)
# =============================================================================
add_executable(test_benchmark_billboard_sorting
    test_benchmark_billboard_sorting.c
    ${CMAKE_SOURCE_DIR}/src/billboard_sorting.c
    ${CMAKE_SOURCE_DIR}/tests/mocks/standalone/mock_gl_standalone.c
    ${CMAKE_SOURCE_DIR}/tests/mocks/standalone/mock_shader_compute.c
    ${CMAKE_SOURCE_DIR}/tests/mocks/standalone/mock_log.c
    ${CMAKE_SOURCE_DIR}/src/platform/platform_utils.c
)
target_sources(test_benchmark_billboard_sorting PRIVATE ${CMAKE_SOURCE_DIR}/src/platform/platform_utils.c)

target_include_directories(test_benchmark_billboard_sorting PRIVATE
    ${CMAKE_SOURCE_DIR}/tests/mocks
    ${CMAKE_SOURCE_DIR}/tests/mocks/standalone
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
    ${stb_SOURCE_DIR}
)

target_link_libraries(test_benchmark_billboard_sorting PRIVATE cglm m)

if(WIN32)
  add_test(NAME test_benchmark_billboard_sorting
             COMMAND ${CMAKE_SOURCE_DIR}/.github/workflows/scripts/run_test_with_xvfb.sh $<TARGET_FILE:test_benchmark_billboard_sorting>)
else()
  add_test(NAME test_benchmark_billboard_sorting
             COMMAND test_benchmark_billboard_sorting)
endif()
# NOTES ET DOCUMENTATION
# =============================================================================
# Cette approche offre plusieurs avantages :
#
# 1. ZÉRO MAINTENANCE - Les tests sont découverts automatiquement
#    Ajouter un test = créer test_foo.c, c'est tout !
#
# 2. FAST COMPILATION - app_testlib compilée une fois, réutilisée partout
#
# 3. CONSISTANCE - Tous les tests utilisent exactement le même code
#
# 4. FLEXIBILITÉ - Facile d'identifier les tests OpenGL vs unitaires
#
# Pour ajouter un nouveau test :
# 1. Créer tests/test_monmodule.c
# 2. Si nécessite OpenGL, l'ajouter dans OPENGL_TESTS
# 3. cmake .. && make (pas besoin de modifier CMakeLists.txt!)
#
# Pour voir tous les tests disponibles :
#   cd build && ctest -N
#
# Pour lancer tous les tests :
#   cd build && ctest --output-on-failure
#
# Pour lancer un test spécifique :
#   cd build && ctest -R test_camera --verbose
# =============================================================================
# =============================================================================
# SPHERE SORTING TESTS (MOCKED)
# =============================================================================
add_executable(test_billboard_sorting
    test_billboard_sorting.c
    ${unity_SOURCE_DIR}/src/unity.c
    ${CMAKE_SOURCE_DIR}/tests/mocks/standalone/mock_gl_standalone.c
    ${CMAKE_SOURCE_DIR}/tests/mocks/standalone/mock_shader_compute.c
    ${CMAKE_SOURCE_DIR}/tests/mocks/standalone/mock_log.c
    ${CMAKE_SOURCE_DIR}/src/billboard_sorting.c
    ${CMAKE_SOURCE_DIR}/src/utils.c
    ${CMAKE_SOURCE_DIR}/src/platform/platform_utils.c
)
target_sources(test_billboard_sorting PRIVATE ${CMAKE_SOURCE_DIR}/src/platform/platform_utils.c)
target_include_directories(test_billboard_sorting PRIVATE
    ${CMAKE_SOURCE_DIR}/tests/mocks
    ${CMAKE_SOURCE_DIR}/tests/mocks/standalone
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
    ${stb_SOURCE_DIR}
    ${unity_SOURCE_DIR}/src
)
target_link_libraries(test_billboard_sorting PRIVATE cglm m)
if(WIN32)
  add_test(NAME test_billboard_sorting
             COMMAND ${CMAKE_SOURCE_DIR}/.github/workflows/scripts/run_test_with_xvfb.sh $<TARGET_FILE:test_billboard_sorting>)
else()
  add_test(NAME test_billboard_sorting COMMAND test_billboard_sorting)
endif()

add_executable(test_benchmark_billboard_sorting_perf
    test_benchmark_billboard_sorting_perf.c
    ${unity_SOURCE_DIR}/src/unity.c
    ${CMAKE_SOURCE_DIR}/tests/mocks/standalone/mock_gl_standalone.c
    ${CMAKE_SOURCE_DIR}/tests/mocks/standalone/mock_shader_compute.c
    ${CMAKE_SOURCE_DIR}/tests/mocks/standalone/mock_log.c
    ${CMAKE_SOURCE_DIR}/src/billboard_sorting.c
    ${CMAKE_SOURCE_DIR}/src/utils.c
)
target_sources(test_benchmark_billboard_sorting_perf PRIVATE ${CMAKE_SOURCE_DIR}/src/platform/platform_utils.c)
target_include_directories(test_benchmark_billboard_sorting_perf PRIVATE
    ${CMAKE_SOURCE_DIR}/tests/mocks
    ${CMAKE_SOURCE_DIR}/tests/mocks/standalone
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
    ${stb_SOURCE_DIR}
    ${unity_SOURCE_DIR}/src
)
target_link_libraries(test_benchmark_billboard_sorting_perf PRIVATE cglm m)
if(WIN32)
  add_test(NAME test_benchmark_billboard_sorting_perf
             COMMAND ${CMAKE_SOURCE_DIR}/.github/workflows/scripts/run_test_with_xvfb.sh $<TARGET_FILE:test_benchmark_billboard_sorting_perf>)
else()
  add_test(NAME test_benchmark_billboard_sorting_perf COMMAND test_benchmark_billboard_sorting_perf)
endif()

# =============================================================================
# SH MATH PRECISION TEST (STANDALONE)
# =============================================================================
add_executable(test_sh_precision
    test_sh_precision.c
    ${CMAKE_SOURCE_DIR}/src/sh_math.c
)
target_include_directories(test_sh_precision PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
)
target_link_libraries(test_sh_precision PRIVATE cglm m)
if(WIN32)
  add_test(NAME test_sh_precision
             COMMAND ${CMAKE_SOURCE_DIR}/.github/workflows/scripts/run_test_with_xvfb.sh $<TARGET_FILE:test_sh_precision>)
else()
  add_test(NAME test_sh_precision COMMAND test_sh_precision)
endif()

# =============================================================================
# SH INTEGRATION TEST (MOCKED)
# =============================================================================
add_executable(test_sh_integration
    test_sh_integration.c
    ${CMAKE_SOURCE_DIR}/src/sh_math.c
    ${CMAKE_SOURCE_DIR}/src/light_probes.c
    ${CMAKE_SOURCE_DIR}/src/render_utils.c
    ${CMAKE_SOURCE_DIR}/src/utils.c
    ${CMAKE_SOURCE_DIR}/src/perf_timer.c
    ${CMAKE_SOURCE_DIR}/src/platform/platform_utils.c
    ${CMAKE_SOURCE_DIR}/tests/mocks/standalone/mock_gl_standalone.c
    ${CMAKE_SOURCE_DIR}/tests/mocks/standalone/mock_log.c
)
target_include_directories(test_sh_integration PRIVATE
    ${CMAKE_SOURCE_DIR}/tests/mocks
    ${CMAKE_SOURCE_DIR}/tests/mocks/standalone
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
)
target_link_libraries(test_sh_integration PRIVATE cglm m pthread)
if(WIN32)
  add_test(NAME test_sh_integration
             COMMAND ${CMAKE_SOURCE_DIR}/.github/workflows/scripts/run_test_with_xvfb.sh $<TARGET_FILE:test_sh_integration>)
else()
  add_test(NAME test_sh_integration COMMAND test_sh_integration)
endif()

# =============================================================================
# ASYNC COORDINATOR TEST (MOCKED)
# =============================================================================
add_executable(test_async_coordinator
    test_async_coordinator.c
    ${unity_SOURCE_DIR}/src/unity.c
    ${CMAKE_SOURCE_DIR}/src/async_coordinator.c
    ${CMAKE_SOURCE_DIR}/src/platform/platform_utils.c
)
target_include_directories(test_async_coordinator PRIVATE
    ${CMAKE_SOURCE_DIR}/tests/mocks/standalone
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
    ${unity_SOURCE_DIR}/src
    ${glad_BINARY_DIR}/include
    ${glfw_SOURCE_DIR}/include
)
target_link_libraries(test_async_coordinator PRIVATE cglm m)
if(WIN32)
  add_test(NAME test_async_coordinator
             COMMAND ${CMAKE_SOURCE_DIR}/.github/workflows/scripts/run_test_with_xvfb.sh $<TARGET_FILE:test_async_coordinator>)
else()
  add_test(NAME test_async_coordinator COMMAND test_async_coordinator)
endif()

# =============================================================================
# ASYNC LOADER TEST (MOCKED)
# =============================================================================
add_executable(test_async_loader
    test_async_loader.c
    ${unity_SOURCE_DIR}/src/unity.c
    ${CMAKE_SOURCE_DIR}/src/async_loader.c
    ${CMAKE_SOURCE_DIR}/src/async_backend.c
    ${CMAKE_SOURCE_DIR}/src/async_backend_stb.c
    ${CMAKE_SOURCE_DIR}/src/async_backend_ktx.c
    ${CMAKE_SOURCE_DIR}/src/perf_timer.c
    ${CMAKE_SOURCE_DIR}/src/utils.c
    ${CMAKE_SOURCE_DIR}/src/platform/platform_time.c
    ${CMAKE_SOURCE_DIR}/src/platform/platform_utils.c
)
target_include_directories(test_async_loader PRIVATE
    ${CMAKE_SOURCE_DIR}/tests/mocks/standalone
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
    ${unity_SOURCE_DIR}/src
    ${glad_BINARY_DIR}/include
    ${stb_SOURCE_DIR}
)
target_compile_definitions(test_async_loader PRIVATE GL_COMMON_NO_GLFW GL_COMMON_NO_GLAD)
if(UNIX)
  target_link_libraries(test_async_loader PRIVATE cglm m pthread dl ktx)
else()
  target_link_libraries(test_async_loader PRIVATE cglm pthread ktx)
endif()
if(WIN32)
  add_test(NAME test_async_loader
             COMMAND ${CMAKE_SOURCE_DIR}/.github/workflows/scripts/run_test_with_xvfb.sh $<TARGET_FILE:test_async_loader>)
else()
  add_test(NAME test_async_loader COMMAND test_async_loader)
endif()
# =============================================================================
# TEXTURE SECURITY TEST (MOCKED)
# =============================================================================
add_executable(test_texture_security
    test_texture_security.c
    ${unity_SOURCE_DIR}/src/unity.c
    ${CMAKE_SOURCE_DIR}/src/texture.c
    ${CMAKE_SOURCE_DIR}/src/log.c
    ${CMAKE_SOURCE_DIR}/src/tracy_log.c
    ${CMAKE_SOURCE_DIR}/src/utils.c
    ${CMAKE_SOURCE_DIR}/src/io.c
    ${CMAKE_SOURCE_DIR}/src/stb_image_impl.c
    ${CMAKE_SOURCE_DIR}/src/platform/platform_utils.c
    ${CMAKE_SOURCE_DIR}/src/platform/platform_time.c
)
target_include_directories(test_texture_security PRIVATE
    ${CMAKE_SOURCE_DIR}/tests/mocks/standalone
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
    ${unity_SOURCE_DIR}/src
    ${glad_BINARY_DIR}/include
    ${stb_SOURCE_DIR}
)
target_compile_definitions(test_texture_security PRIVATE GL_COMMON_NO_GLFW GL_COMMON_NO_GLAD)
if(UNIX)
  target_link_libraries(test_texture_security PRIVATE cglm m pthread dl)
else()
  target_link_libraries(test_texture_security PRIVATE cglm pthread)
endif()
if(WIN32)
  add_test(NAME test_texture_security
             COMMAND ${CMAKE_SOURCE_DIR}/.github/workflows/scripts/run_test_with_xvfb.sh $<TARGET_FILE:test_texture_security>)
else()
  add_test(NAME test_texture_security COMMAND test_texture_security)
endif()
# =============================================================================
# SCENE N-BODY (STANDALONE MOCKED)
# =============================================================================
add_executable(test_scene_nbody
    test_scene_nbody.c
    ${unity_SOURCE_DIR}/src/unity.c
    ${CMAKE_SOURCE_DIR}/tests/mocks/standalone/mock_gl_standalone.c
    ${CMAKE_SOURCE_DIR}/src/nbody/nbody_physics.c
    ${CMAKE_SOURCE_DIR}/src/nbody/nbody_preset.c
    ${CMAKE_SOURCE_DIR}/src/nbody/nbody_diagnostics.c
    ${CMAKE_SOURCE_DIR}/src/nbody/nbody_render.c
    ${CMAKE_SOURCE_DIR}/src/utils.c
    ${CMAKE_SOURCE_DIR}/src/platform/platform_utils.c
)
if(ENABLE_TRACY)
  target_sources(test_scene_nbody PRIVATE "${CMAKE_SOURCE_DIR}/deps/tracy/public/TracyClient.cpp")
endif()

target_include_directories(test_scene_nbody PRIVATE
    ${CMAKE_SOURCE_DIR}/tests/mocks
    ${CMAKE_SOURCE_DIR}/tests/mocks/standalone
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
    ${stb_SOURCE_DIR}
    ${unity_SOURCE_DIR}/src
)
if(ENABLE_TRACY)
  target_include_directories(test_scene_nbody PRIVATE "${CMAKE_SOURCE_DIR}/deps/tracy/public")
endif()

if(UNIX AND NOT APPLE)
  target_link_libraries(test_scene_nbody PRIVATE cglm m)
else()
  target_link_libraries(test_scene_nbody PRIVATE cglm)
endif()

if(WIN32)
  add_test(NAME test_scene_nbody
             COMMAND ${CMAKE_SOURCE_DIR}/.github/workflows/scripts/run_test_with_xvfb.sh $<TARGET_FILE:test_scene_nbody>)
else()
  add_test(NAME test_scene_nbody COMMAND test_scene_nbody)
endif()

# =============================================================================
# SHOCKWAVE (STANDALONE MOCKED)
# =============================================================================
add_executable(test_shockwave
    test_shockwave.c
    ${unity_SOURCE_DIR}/src/unity.c
    ${CMAKE_SOURCE_DIR}/tests/mocks/standalone/mock_gl_standalone.c
    ${CMAKE_SOURCE_DIR}/tests/mocks/standalone/mock_log.c
    ${CMAKE_SOURCE_DIR}/src/utils.c
    ${CMAKE_SOURCE_DIR}/src/platform/platform_utils.c
)
if(ENABLE_TRACY)
  target_sources(test_shockwave PRIVATE "${CMAKE_SOURCE_DIR}/deps/tracy/public/TracyClient.cpp")
endif()

target_include_directories(test_shockwave PRIVATE
    ${CMAKE_SOURCE_DIR}/tests/mocks
    ${CMAKE_SOURCE_DIR}/tests/mocks/standalone
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
    ${stb_SOURCE_DIR}
    ${unity_SOURCE_DIR}/src
    ${glad_BINARY_DIR}/include
)
if(ENABLE_TRACY)
  target_include_directories(test_shockwave PRIVATE "${CMAKE_SOURCE_DIR}/deps/tracy/public")
endif()

if(UNIX AND NOT APPLE)
  target_link_libraries(test_shockwave PRIVATE cglm m)
else()
  target_link_libraries(test_shockwave PRIVATE cglm)
endif()

if(WIN32)
  add_test(NAME test_shockwave
             COMMAND ${CMAKE_SOURCE_DIR}/.github/workflows/scripts/run_test_with_xvfb.sh $<TARGET_FILE:test_shockwave>)
else()
  add_test(NAME test_shockwave COMMAND test_shockwave)
endif()

# =============================================================================
# TRAIL RENDERER (STANDALONE MOCKED)
# =============================================================================
add_executable(test_trail_renderer
    test_trail_renderer.c
    ${unity_SOURCE_DIR}/src/unity.c
    ${CMAKE_SOURCE_DIR}/tests/mocks/standalone/mock_gl_standalone.c
    ${CMAKE_SOURCE_DIR}/tests/mocks/standalone/mock_log.c
    ${CMAKE_SOURCE_DIR}/tests/mocks/standalone/mock_gpu_profiler.c
    ${CMAKE_SOURCE_DIR}/src/utils.c
    ${CMAKE_SOURCE_DIR}/src/platform/platform_utils.c
)
if(ENABLE_TRACY)
  target_sources(test_trail_renderer PRIVATE "${CMAKE_SOURCE_DIR}/deps/tracy/public/TracyClient.cpp")
endif()

target_include_directories(test_trail_renderer PRIVATE
    ${CMAKE_SOURCE_DIR}/tests/mocks
    ${CMAKE_SOURCE_DIR}/tests/mocks/standalone
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
    ${stb_SOURCE_DIR}
    ${unity_SOURCE_DIR}/src
    ${glad_BINARY_DIR}/include
)
if(ENABLE_TRACY)
  target_include_directories(test_trail_renderer PRIVATE "${CMAKE_SOURCE_DIR}/deps/tracy/public")
endif()

if(UNIX AND NOT APPLE)
  target_link_libraries(test_trail_renderer PRIVATE cglm m)
else()
  target_link_libraries(test_trail_renderer PRIVATE cglm)
endif()

if(WIN32)
  add_test(NAME test_trail_renderer
             COMMAND ${CMAKE_SOURCE_DIR}/.github/workflows/scripts/run_test_with_xvfb.sh $<TARGET_FILE:test_trail_renderer>)
else()
  add_test(NAME test_trail_renderer COMMAND test_trail_renderer)
endif()

# =============================================================================
# POSTPROCESS INPUT (STANDALONE MOCKED)
# =============================================================================
add_executable(test_postprocess_input
    test_postprocess_input.c
    ${unity_SOURCE_DIR}/src/unity.c
    ${CMAKE_SOURCE_DIR}/tests/mocks/standalone/mock_gl_standalone.c
    ${CMAKE_SOURCE_DIR}/tests/mocks/standalone/mock_log.c
    ${CMAKE_SOURCE_DIR}/src/postprocess_presets.c
    ${CMAKE_SOURCE_DIR}/src/utils.c
    ${CMAKE_SOURCE_DIR}/src/platform/platform_utils.c
)
if(ENABLE_TRACY)
  target_sources(test_postprocess_input PRIVATE "${CMAKE_SOURCE_DIR}/deps/tracy/public/TracyClient.cpp")
endif()

target_include_directories(test_postprocess_input PRIVATE
    ${CMAKE_SOURCE_DIR}/tests/mocks
    ${CMAKE_SOURCE_DIR}/tests/mocks/standalone
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
    ${glfw_SOURCE_DIR}/include
    ${stb_SOURCE_DIR}
    ${unity_SOURCE_DIR}/src
    ${glad_BINARY_DIR}/include
)
if(ENABLE_TRACY)
  target_include_directories(test_postprocess_input PRIVATE "${CMAKE_SOURCE_DIR}/deps/tracy/public")
endif()

if(UNIX AND NOT APPLE)
  target_link_libraries(test_postprocess_input PRIVATE cglm m)
else()
  target_link_libraries(test_postprocess_input PRIVATE cglm)
endif()

if(WIN32)
  add_test(NAME test_postprocess_input
             COMMAND ${CMAKE_SOURCE_DIR}/.github/workflows/scripts/run_test_with_xvfb.sh $<TARGET_FILE:test_postprocess_input>)
else()
  add_test(NAME test_postprocess_input COMMAND test_postprocess_input)
endif()

# =============================================================================
# SCENE RENDER (STANDALONE MOCKED)
# =============================================================================
add_executable(test_scene_render
    test_scene_render.c
    ${unity_SOURCE_DIR}/src/unity.c
    ${CMAKE_SOURCE_DIR}/tests/mocks/standalone/mock_gl_standalone.c
    ${CMAKE_SOURCE_DIR}/tests/mocks/standalone/mock_log.c
    ${CMAKE_SOURCE_DIR}/src/utils.c
    ${CMAKE_SOURCE_DIR}/src/platform/platform_utils.c
)
if(ENABLE_TRACY)
  target_sources(test_scene_render PRIVATE "${CMAKE_SOURCE_DIR}/deps/tracy/public/TracyClient.cpp")
endif()

target_include_directories(test_scene_render PRIVATE
    ${CMAKE_SOURCE_DIR}/tests/mocks
    ${CMAKE_SOURCE_DIR}/tests/mocks/standalone
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
    ${stb_SOURCE_DIR}
    ${unity_SOURCE_DIR}/src
    ${glad_BINARY_DIR}/include
)
if(ENABLE_TRACY)
  target_include_directories(test_scene_render PRIVATE "${CMAKE_SOURCE_DIR}/deps/tracy/public")
endif()

if(UNIX AND NOT APPLE)
  target_link_libraries(test_scene_render PRIVATE cglm m)
else()
  target_link_libraries(test_scene_render PRIVATE cglm)
endif()

if(WIN32)
  add_test(NAME test_scene_render
             COMMAND ${CMAKE_SOURCE_DIR}/.github/workflows/scripts/run_test_with_xvfb.sh $<TARGET_FILE:test_scene_render>)
else()
  add_test(NAME test_scene_render COMMAND test_scene_render)
endif()

# =============================================================================
# IBL EXTRACTION TOOL (STANDALONE)
# =============================================================================
add_executable(test_ibl_extract test_ibl_extract.c)
target_link_libraries(test_ibl_extract PRIVATE app_testlib)
# No add_test because it's a CLI tool for verification, not a unit test.
