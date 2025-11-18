include(FetchContent)

FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
	GIT_TAG 1.0.2
	GIT_SHALLOW TRUE)

FetchContent_MakeAvailable(glm)

add_library(GLM INTERFACE)

set(GLM_DIR ${glm_SOURCE_DIR})

target_include_directories(GLM INTERFACE ${GLM_DIR})

target_compile_definitions(GLM INTERFACE
    GLM_FORCE_SWIZZLE
    GLM_FORCE_RADIANS
    GLM_FORCE_CTOR_INIT
    GLM_ENABLE_EXPERIMENTAL
    GLM_FORCE_DEPTH_ZERO_TO_ONE
)

set_property(TARGET glm PROPERTY FOLDER "Dependencies")
