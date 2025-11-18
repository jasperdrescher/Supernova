file(GLOB_RECURSE PROJECT_SOURCES
    Source/*.cpp)

file(GLOB_RECURSE PROJECT_INCLUDES
    Source/*.hpp)

add_executable(Editor ${PROJECT_SOURCES} ${PROJECT_INCLUDES})

if (MSVC)
    target_compile_options(Editor PRIVATE /W4)
endif()

target_link_libraries(Editor PUBLIC
		Engine)

target_include_directories(Editor
	PUBLIC
        ${PROJECT_SOURCE_DIR}
		 ${GLFW_INCLUDE_DIR}
		 ${IMGUI_INCLUDE_DIR})

set_property(TARGET Editor PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${PROJETC_SOURCE_ROOT_DIR}")

set(RESOURCES_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Resources/")
set(RESOURCES_DESTINATION_DIR "${CMAKE_CURRENT_BINARY_DIR}/Resources/")
add_custom_command(
    TARGET Editor POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different ${RESOURCES_SOURCE_DIR} ${RESOURCES_DESTINATION_DIR}
    COMMENT "Copying resources ${RESOURCES_SOURCE_DIR} => ${RESOURCES_DESTINATION_DIR}")
