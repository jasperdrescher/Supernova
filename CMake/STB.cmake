include(FetchContent)

FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG master
	GIT_SHALLOW TRUE)

FetchContent_MakeAvailable(stb)

FetchContent_GetProperties(stb)

add_library(STB INTERFACE)

target_include_directories(STB INTERFACE ${stb_SOURCE_DIR})
