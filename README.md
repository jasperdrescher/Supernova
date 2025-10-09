# Supernova
The goal is to develop a small game engine that can render 2D and 3D scenes using Vulkan. This also includes systems for glTF scenes, physics, entities, and input.

## Demo
<a href='https://postimg.cc/TpmbDpzs' target='_blank'><img src='https://i.postimg.cc/TpmbDpzs/Supernova-Rendering.gif' border='0' alt='Supernova-Rendering'/></a>
<sub>*Captured on 2025/10/09*</sub>

## Graphics
The renderer is based on [Vulkan C++ examples and demos](https://github.com/SaschaWillems/Vulkan) by Sascha Willems and is tested on AMD and Nvidia graphics drivers.
Currently using Vulkan [1.3](https://registry.khronos.org/vulkan/specs/latest/html/vkspec.html#versions-1.3) with [Dynamic Rendering](https://registry.khronos.org/vulkan/specs/latest/html/vkspec.html#_render_pass_objects_deprecation_via_dynamic_rendering).

## Dependencies
The following third-party dependencies are being used.
| Dependency  | Version | Purpose 
| ------------- | ------------- | ------------- |
| GLFW  | 3.4 | Cross-platform window management |
| GLM  | 1.0.1  | Graphics math |
| Dear ImGui  | 1.92.3  | GUI |
| TinyGLTF  | 2.9.6  | glTF loading |
| EnTT  | 3.15.0  | Entity-component system |
| KTX  | 4.4.0  | Vulkan textures |
| stb_image  | 1.16  | Image loading |

## Compilation
The project uses C++20 with the latest installed Windows 11 SDK (10.0.x) for compilation.
Visual Studio 2022 is supported, but CMake support will be added soon.
