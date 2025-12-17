include(FetchContent)

list(PREPEND CMAKE_MODULE_PATH "${PROJETC_SOURCE_ROOT_DIR}/CMake")

include(STB)
include(GLM)
include(TinyGLTF)
include(EnTT)
include(GLFW)
include(Ktx)
include(ImGui)

if(USE_VULKAN_C)
	include(VulkanC)
else()
	include(VulkanHpp)
endif()

file(GLOB_RECURSE PROJECT_SOURCES
    Source/*.cpp)

file(GLOB_RECURSE PROJECT_INCLUDES
    Source/*.hpp)

add_library(Engine STATIC ${PROJECT_SOURCES} ${PROJECT_INCLUDES})

set_property(TARGET Engine PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${PROJETC_SOURCE_ROOT_DIR}")

if (MSVC)
    target_compile_options(Engine PRIVATE
		/W4
		"/MP")
endif()

if(USE_VULKAN_C)
	target_compile_definitions(Engine PUBLIC
		"VULKAN_C")
else()
	target_compile_definitions(Engine PUBLIC
		VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1)
endif()

if(USE_VULKAN_C)
	target_link_libraries(Engine
		PUBLIC
			ktx
			glfw
			EnTT
			TinyGLTF
			GLM
			ImGui
			Vulkan::Vulkan)
else()
	target_link_libraries(Engine
		PUBLIC
			ktx
			glfw
			EnTT
			TinyGLTF
			GLM
			ImGui
			Vulkan::Headers)
endif()

target_include_directories(Engine
	PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/Source/"
		${GLFW_INCLUDE_DIR})

set(RESOURCES_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Resources/")
set(RESOURCES_DESTINATION_DIR "${CMAKE_CURRENT_BINARY_DIR}/Resources/")
add_custom_command(
    TARGET Engine POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different ${RESOURCES_SOURCE_DIR} ${RESOURCES_DESTINATION_DIR}
    COMMENT "Copying resources ${RESOURCES_SOURCE_DIR} => ${RESOURCES_DESTINATION_DIR}")
