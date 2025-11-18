cmake_minimum_required(VERSION 3.26 FATAL_ERROR)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(CMAKE_CXX_STANDARD 20)

set(PROJETC_SOURCE_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(PROJECT_BINARY_ROOT_DIR ${CMAKE_CURRENT_BINARY_DIR})

project("Supernova" DESCRIPTION "Supernova Game Engine" VERSION 0.1.0 LANGUAGES C CXX)

add_subdirectory("Editor")
add_subdirectory("Engine")
