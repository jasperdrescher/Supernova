include(FetchContent)

FetchContent_Declare(
    ktx
    GIT_REPOSITORY https://github.com/KhronosGroup/KTX-Software.git
    GIT_TAG v4.4.2
    GIT_SHALLOW TRUE
)

set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(KTX_FEATURE_TOOLS OFF CACHE INTERNAL "")
set(KTX_FEATURE_TOOLS_CTS OFF CACHE INTERNAL "")
set(KTX_FEATURE_TESTS OFF CACHE INTERNAL "")
set(KTX_FEATURE_DOC OFF CACHE INTERNAL "")
set(KTX_FEATURE_JNI OFF CACHE INTERNAL "")
set(KTX_FEATURE_PY OFF CACHE INTERNAL "")
set(KTX_FEATURE_LOADTEST_APPS OFF CACHE INTERNAL "")
set(KTX_FEATURE_LOADTESTS OFF CACHE INTERNAL "")
set(KTX_FEATURE_GL_UPLOAD OFF CACHE INTERNAL "")
set(KTX_FEATURE_STATIC_LIBRARY ON INTERNAL)
set(KTX_FEATURE_VULKAN ON CACHE INTERNAL "")
set(KTX_FEATURE_KTX2 ON CACHE INTERNAL "")

FetchContent_GetProperties(ktx)

if(NOT ktx_POPULATED)
  FetchContent_Populate(ktx)
  add_subdirectory(${ktx_SOURCE_DIR} ${ktx_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()

set_property(TARGET ktx PROPERTY FOLDER "Dependencies/KTX")
set_property(TARGET ktx_read PROPERTY FOLDER "Dependencies/KTX")
set_property(TARGET ktx_version PROPERTY FOLDER "Dependencies/KTX")
set_property(TARGET obj_basisu_cbind PROPERTY FOLDER "Dependencies/KTX")
set_property(TARGET objUtil PROPERTY FOLDER "Dependencies/KTX")
set_property(TARGET astcenc-avx2-static PROPERTY FOLDER "Dependencies/KTX")
