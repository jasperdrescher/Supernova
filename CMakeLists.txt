cmake_minimum_required(VERSION 3.26 FATAL_ERROR)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

set(CMAKE_CXX_STANDARD 20)
set(USE_FOLDERS true)

set(PROJETC_SOURCE_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(PROJECT_BINARY_ROOT_DIR ${CMAKE_CURRENT_BINARY_DIR})

project("Supernova" DESCRIPTION "Supernova Game Engine" VERSION 0.1.0 LANGUAGES C CXX)

add_subdirectory("Editor")
add_subdirectory("Engine")
