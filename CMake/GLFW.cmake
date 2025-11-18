include(FetchContent)

FetchContent_Declare(
	GLFW
	GIT_REPOSITORY https://github.com/glfw/glfw.git
	GIT_TAG 3.4
	GIT_SHALLOW TRUE)

set(BUILD_SHARED_LIBS OFF CACHE INTERNAL "")
set(GLFW_BUILD_DOCS OFF CACHE INTERNAL "")
set(GLFW_BUILD_TESTS OFF CACHE INTERNAL "")
set(GLFW_BUILD_EXAMPLES OFF CACHE INTERNAL "")
set(GLFW_INSTALL OFF CACHE INTERNAL "")
set(GLFW_USE_HYBRID_HPG ON CACHE INTERNAL "")

FetchContent_MakeAvailable(GLFW)

set_property(TARGET glfw PROPERTY FOLDER "Dependencies/GLFW")
set_property(TARGET update_mappings PROPERTY FOLDER "Dependencies/GLFW")
