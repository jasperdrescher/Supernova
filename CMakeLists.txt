cmake_minimum_required(VERSION 3.30 FATAL_ERROR)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(CMAKE_CXX_STANDARD 20)
set(CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(PROJETC_SOURCE_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(PROJECT_BINARY_ROOT_DIR ${CMAKE_CURRENT_BINARY_DIR})

option(USE_VULKAN_C "Use Vulkan C instead of HPP" OFF)

project("Supernova" DESCRIPTION "Supernova Game Engine" VERSION 0.1.0 LANGUAGES C CXX)

add_subdirectory("Editor")
add_subdirectory("Engine")
