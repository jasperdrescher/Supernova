include(FetchContent)

FetchContent_Declare(
	tinygltf
	GIT_REPOSITORY https://github.com/syoyo/tinygltf.git
	GIT_TAG v2.9.7
	GIT_SHALLOW TRUE)

set(TINYGLTF_BUILD_LOADER_EXAMPLE OFF CACHE INTERNAL "" FORCE)
set(TINYGLTF_INSTALL OFF CACHE INTERNAL "" FORCE)
set(TINYGLTF_HEADER_ONLY ON CACHE INTERNAL "" FORCE)

FetchContent_MakeAvailable(tinygltf)

add_library(TinyGLTF INTERFACE)

target_include_directories(TinyGLTF INTERFACE ${tinygltf_SOURCE_DIR})
