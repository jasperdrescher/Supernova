# Supernova
The goal is to develop a small game engine along with a level editor that can render 2D and 3D scenes using Vulkan.
The engine includes several systems:
- Entity-component system
- Physics
- Animation
- User input
- Renderer
- glTF model loading
- Scenes
- Event system

## Demo
<a href='https://postimg.cc/TpmbDpzs' target='_blank'><img src='https://i.postimg.cc/TpmbDpzs/Supernova-Rendering.gif' border='0' alt='Supernova-Rendering'/></a>
<sub>*Captured on 2025/10/09*</sub>

## Graphics
The renderer is based on [Vulkan C++ examples and demos by Sascha Willems](https://github.com/SaschaWillems/Vulkan) and [Vulkan Samples by Khronos Group](https://github.com/KhronosGroup/Vulkan-Samples/tree/main) and is tested on AMD and Nvidia graphics drivers.
Currently using Vulkan [1.4](https://registry.khronos.org/vulkan/specs/latest/html/vkspec.html#versions-1.4) with [Dynamic Rendering](https://registry.khronos.org/vulkan/specs/latest/html/vkspec.html#_render_pass_objects_deprecation_via_dynamic_rendering).

## Dependencies
All third-party dependencies can be found in the [CMake](https://github.com/jasperdrescher/Supernova/tree/main/CMake) folder, which are pulled using FetchContent.

## Compilation
The project uses C++20, CMake, Ninja, and Clang to generate and build the project.
