find_package(Vulkan REQUIRED COMPONENTS glslc)
find_program(glslc_executable NAMES glslc HINTS Vulkan::glslc)

if(${Vulkan_VERSION} VERSION_LESS "1.3.256")
  message( FATAL_ERROR "Minimum required Vulkan version for C++ modules is 1.3.256. "
           "Found ${Vulkan_VERSION}.")
endif()

add_library(VulkanHppModule)

target_sources(VulkanHppModule PRIVATE
  FILE_SET CXX_MODULES
  BASE_DIRS ${Vulkan_INCLUDE_DIR}
  FILES ${Vulkan_INCLUDE_DIR}/vulkan/vulkan.cppm)

target_compile_features(VulkanHppModule PUBLIC cxx_std_20)

target_link_libraries(VulkanHppModule PUBLIC Vulkan::Headers)

set_property(TARGET VulkanHppModule PROPERTY FOLDER "Dependencies")
