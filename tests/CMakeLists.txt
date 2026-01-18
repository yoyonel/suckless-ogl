# tests/CMakeLists.txt - Mise à jour complète

# Common dependencies for all tests
set(TEST_COMMON_LIBS unity cglm glad cjson ${GLFW3_LIBRARIES} m dl)
set(TEST_INCLUDE_DIRS ../src ../include ${stb_SOURCE_DIR} ${cjson_SOURCE_DIR})


# Helper macro to add a test executable
macro(add_unit_test test_name sources)
    add_executable(${test_name} ${test_name}.c ${sources})
    target_link_libraries(${test_name} PRIVATE ${TEST_COMMON_LIBS})
    target_include_directories(${test_name} PRIVATE ${TEST_INCLUDE_DIRS})
    target_compile_definitions(${test_name} PRIVATE GLFW_INCLUDE_NONE _POSIX_C_SOURCE=199309L)
    add_test(NAME ${test_name} COMMAND ${test_name})
    # Set working directory to root so assets/ and shaders/ are found
    set_tests_properties(${test_name} PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
endmacro()

# Helper macro for OpenGL tests
macro(add_xvfb_test test_name sources)
    add_executable(${test_name} ${test_name}.c ${sources})
    target_link_libraries(${test_name} PRIVATE ${TEST_COMMON_LIBS})
    target_include_directories(${test_name} PRIVATE ${TEST_INCLUDE_DIRS})
    target_compile_definitions(${test_name} PRIVATE GLFW_INCLUDE_NONE _POSIX_C_SOURCE=199309L)
    
    add_test(NAME ${test_name} 
             COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/run_test_with_xvfb.sh $<TARGET_FILE:${test_name}>)
    # Set working directory to root
    set_tests_properties(${test_name} PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
endmacro()

# Tests existants (sans OpenGL)
add_unit_test(test_icosphere "../src/icosphere.c")
add_unit_test(test_log "../src/log.c")
add_unit_test(test_shader "../src/shader.c;../src/log.c")
add_unit_test(test_skybox "../src/skybox.c;../src/shader.c;../src/log.c;../src/texture.c;../src/stb_image_impl.c")
add_unit_test(test_texture "../src/texture.c;../src/log.c;../src/stb_image_impl.c")

# Tests sans dépendances OpenGL
add_unit_test(test_camera "../src/camera.c")
add_unit_test(test_fps "../src/fps.c")
add_unit_test(test_perf_timer "../src/perf_timer.c;../src/log.c")
add_unit_test(test_stb_image_impl "../src/stb_image_impl.c")

# Tests nécessitant un contexte OpenGL (avec Xvfb)
add_xvfb_test(test_app "../src/app.c;../src/shader.c;../src/log.c;../src/icosphere.c;../src/skybox.c;../src/texture.c;../src/stb_image_impl.c;../src/fps.c;../src/ui.c;../src/pbr.c;../src/material.c;../src/perf_timer.c;../src/instanced_rendering.c;../src/ssbo_rendering.c;../src/camera.c")

add_xvfb_test(test_pbr "../src/pbr.c;../src/shader.c;../src/log.c;../src/material.c")

add_xvfb_test(test_material "../src/material.c;../src/log.c")

add_xvfb_test(test_ui "../src/ui.c;../src/shader.c;../src/log.c;../src/stb_image_impl.c")

add_xvfb_test(test_instanced_rendering "../src/instanced_rendering.c;../src/log.c")

add_xvfb_test(test_ssbo_rendering "../src/ssbo_rendering.c;../src/log.c")

# NOTE: test_main est commenté car main.c contient main() qui entre en conflit avec Unity
# Pour tester main.c, il faudrait refactoriser le code ou utiliser une approche différente
# add_unit_test(test_main "../src/main.c;...")