cmake_minimum_required(VERSION 3.14)
project(ktx_viewer C)

# ---------------------------------------------------------------------------
# CMakeLists autonome pour le viewer KTX2 (mini-outil standalone).
#
# Objectif : build ULTRA-RAPIDE — aucun FetchContent, aucun test Unity.
# On réutilise les artefacts déjà compilés par le build principal (build/).
#
# Pré-requis : exécuter `just build` au moins une fois pour peupler
#   build/_deps/glad-build/
#   build/_deps/glfw-build/
#   build/_deps/libktx-build/
#
# Sources : src/ktx_viewer.c  +  src/utils.c
# Headers : include/utils.h
# ---------------------------------------------------------------------------

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# --- Racine du projet (ce fichier est dans cmake/ktx_viewer/) ---------------
set(PROJECT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../..")
set(DEPS_ROOT    "${PROJECT_ROOT}/build/_deps")

# --- GLAD -------------------------------------------------------------------
set(GLAD_INCLUDE_DIR "${DEPS_ROOT}/glad-build/include")
set(GLAD_LIB         "${DEPS_ROOT}/glad-build/libglad.a")
if(NOT EXISTS "${GLAD_INCLUDE_DIR}/glad/glad.h")
    message(FATAL_ERROR
        "GLAD introuvable dans ${GLAD_INCLUDE_DIR}.\n"
        "Exécutez 'just build' (build principal) au moins une fois d'abord."
    )
endif()

# --- GLFW3 ------------------------------------------------------------------
set(GLFW_INCLUDE_DIR "${DEPS_ROOT}/glfw-src/include")
set(GLFW_LIB         "${DEPS_ROOT}/glfw-build/src/libglfw3.a")
if(NOT EXISTS "${GLFW_INCLUDE_DIR}/GLFW/glfw3.h")
    message(FATAL_ERROR
        "GLFW introuvable dans ${GLFW_INCLUDE_DIR}.\n"
        "Exécutez 'just build' (build principal) au moins une fois d'abord."
    )
endif()

# --- libktx -----------------------------------------------------------------
set(KTX_INCLUDE_DIR  "/usr/include")
set(KTX_LIB          "${DEPS_ROOT}/libktx-build/lib/libktx.a")
if(NOT EXISTS "${KTX_LIB}")
    # Fallback : lib système
    find_library(KTX_LIB_SYS NAMES ktx)
    if(KTX_LIB_SYS)
        set(KTX_LIB "${KTX_LIB_SYS}")
        message(STATUS "[ktx_viewer] libktx : système (${KTX_LIB})")
    else()
        message(FATAL_ERROR
            "libktx introuvable. Exécutez 'just build' ou installez libktx-dev."
        )
    endif()
endif()

# --- Threads (requis par libktx) --------------------------------------------
find_package(Threads REQUIRED)

message(STATUS "[ktx_viewer] GLAD    include : ${GLAD_INCLUDE_DIR}")
message(STATUS "[ktx_viewer] GLAD    lib     : ${GLAD_LIB}")
message(STATUS "[ktx_viewer] GLFW    include : ${GLFW_INCLUDE_DIR}")
message(STATUS "[ktx_viewer] GLFW    lib     : ${GLFW_LIB}")
message(STATUS "[ktx_viewer] libktx  include : ${KTX_INCLUDE_DIR}")
message(STATUS "[ktx_viewer] libktx  lib     : ${KTX_LIB}")

# --- Target -----------------------------------------------------------------
add_executable(ktx_viewer
    "${PROJECT_ROOT}/src/ktx_viewer.c"
    "${PROJECT_ROOT}/src/utils.c"
)

target_include_directories(ktx_viewer PRIVATE
    "${PROJECT_ROOT}/include"
    "${KTX_INCLUDE_DIR}"
    "${GLAD_INCLUDE_DIR}"
    "${GLFW_INCLUDE_DIR}"
)

target_compile_definitions(ktx_viewer PRIVATE
    GLFW_INCLUDE_NONE
    _GNU_SOURCE
    _POSIX_C_SOURCE=200809L
)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "" FORCE)
endif()

if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(ktx_viewer PRIVATE
        -Wall -Wextra -Wpedantic
    )
endif()

# GLFW sur Linux nécessite X11, dl, pthread
target_link_libraries(ktx_viewer PRIVATE
    "${GLAD_LIB}"
    "${GLFW_LIB}"
    "${KTX_LIB}"
    Threads::Threads
)

if(UNIX)
    target_link_libraries(ktx_viewer PRIVATE m dl stdc++)
    # X11 pour GLFW (détection de ce qui est réellement présent)
    find_package(X11)
    if(X11_FOUND)
        target_link_libraries(ktx_viewer PRIVATE X11::X11)
    endif()
endif()

message(STATUS "[ktx_viewer] Target configurée — type: ${CMAKE_BUILD_TYPE}")
