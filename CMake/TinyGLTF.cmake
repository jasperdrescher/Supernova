include(FetchContent)

FetchContent_Declare(
	tinygltf
	GIT_REPOSITORY https://github.com/syoyo/tinygltf.git
	GIT_TAG v2.9.7
	GIT_SHALLOW TRUE)

FetchContent_MakeAvailable(tinygltf)

FetchContent_GetProperties(tinygltf)

add_library(TinyGLTF INTERFACE)

target_include_directories(TinyGLTF INTERFACE ${tinygltf_SOURCE_DIR})
