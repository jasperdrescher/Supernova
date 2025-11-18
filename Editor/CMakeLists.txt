file(GLOB_RECURSE PROJECT_SOURCES
    Source/*.cpp)

file(GLOB_RECURSE PROJECT_INCLUDES
    Source/*.hpp)

add_executable(Editor ${PROJECT_SOURCES} ${PROJECT_INCLUDES})

if (MSVC)
    target_compile_options(Editor PRIVATE /W3)
endif()

target_link_libraries(Editor PUBLIC
		Engine)

target_include_directories(Editor
	PUBLIC
        ${PROJECT_SOURCE_DIR}
		"${CMAKE_SOURCE_DIR}/Dependencies/"
		 ${GLFW_INCLUDE_DIR}
		 ${IMGUI_INCLUDE_DIR})

set_property(TARGET Editor PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${projectSourceRoot}")
