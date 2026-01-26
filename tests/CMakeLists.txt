# tests/CMakeLists.txt - Version avec auto-découverte des tests

# =============================================================================
# CONFIGURATION COMMUNE
# =============================================================================

set(TEST_COMMON_LIBS unity cglm glad cjson ${GLFW3_LIBRARIES} m dl)
set(TEST_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/include ${stb_SOURCE_DIR} ${cjson_SOURCE_DIR})

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
add_library(app_testlib STATIC ${TEST_LIB_SOURCES})
target_include_directories(app_testlib PUBLIC ${TEST_INCLUDE_DIRS})
target_link_libraries(app_testlib PUBLIC ${TEST_COMMON_LIBS})
# target_compile_definitions(app_testlib PUBLIC GLFW_INCLUDE_NONE _POSIX_C_SOURCE=199309L)

# =============================================================================
# AUTO-DÉCOUVERTE DES TESTS
# =============================================================================

# Lister tous les fichiers test_*.c dans le répertoire tests/
file(GLOB TEST_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/test_*.c")

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
    test_stability
)

# Pour chaque fichier test trouvé
foreach(test_file ${TEST_SOURCES})
    # Extraire le nom du test (sans .c)
    get_filename_component(test_name ${test_file} NAME_WE)

    # Créer l'exécutable
    add_executable(${test_name} ${test_file})
    target_link_libraries(${test_name} PRIVATE app_testlib)

    # Déterminer si c'est un test OpenGL
    if(${test_name} IN_LIST OPENGL_TESTS)
        # Test avec Xvfb
        add_test(NAME ${test_name}
                 COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/run_test_with_xvfb.sh $<TARGET_FILE:${test_name}>)
        message(STATUS "Test OpenGL découvert: ${test_name} (avec Xvfb)")
    else()
        # Test simple
        add_test(NAME ${test_name} COMMAND ${test_name})
        message(STATUS "Test unitaire découvert: ${test_name}")
    endif()

    # Working directory = racine du projet
    set_tests_properties(${test_name} PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
endforeach()

# =============================================================================
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
