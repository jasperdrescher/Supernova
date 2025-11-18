include(FetchContent)

find_package(Vulkan REQUIRED COMPONENTS glslc)
find_program(glslc_executable NAMES glslc HINTS Vulkan::glslc)

list(PREPEND CMAKE_MODULE_PATH "${PROJETC_SOURCE_ROOT_DIR}/CMake")
include(STB)
include(GLM)
include(TinyGLTF)
include(EnTT)
include(GLFW)
include(Ktx)
include(ImGui)

file(GLOB_RECURSE PROJECT_SOURCES
    Source/*.cpp)

file(GLOB_RECURSE PROJECT_INCLUDES
    Source/*.hpp)

add_library(Engine STATIC ${PROJECT_SOURCES} ${PROJECT_INCLUDES})

set_property(TARGET Engine PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${PROJETC_SOURCE_ROOT_DIR}")

if (MSVC)
    target_compile_options(Engine PRIVATE /W4)
endif()

target_link_libraries(Engine
	PUBLIC
		Vulkan::Vulkan
        ktx
		glfw
        EnTT
        TinyGLTF
        GLM
		ImGui)

target_include_directories(Engine
	PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/Source/"
        ${VULKAN_INCLUDE_DIR}
		${GLFW_INCLUDE_DIR})

set(RESOURCES_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Resources/")
set(RESOURCES_DESTINATION_DIR "${CMAKE_CURRENT_BINARY_DIR}/Resources/")
add_custom_command(
    TARGET Engine POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different ${RESOURCES_SOURCE_DIR} ${RESOURCES_DESTINATION_DIR}
    COMMENT "Copying resources ${RESOURCES_SOURCE_DIR} => ${RESOURCES_DESTINATION_DIR}")
