find_package(Vulkan REQUIRED COMPONENTS glslc)
find_program(glslc_executable NAMES glslc HINTS Vulkan::glslc)

if(${Vulkan_VERSION} VERSION_LESS "1.3.256" )
  message( FATAL_ERROR "Minimum required Vulkan version for C++ modules is 1.3.256. "
           "Found ${Vulkan_VERSION}.")
endif()

add_library(VulkanHpp INTERFACE)

target_include_directories(VulkanHpp INTERFACE ${Vulkan_INCLUDE_DIR})

target_compile_definitions(VulkanHpp INTERFACE VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1)

target_compile_features(VulkanHpp INTERFACE cxx_std_20)

target_link_libraries(VulkanHpp INTERFACE Vulkan::Headers)

set_property(TARGET VulkanHpp PROPERTY FOLDER "Dependencies")
