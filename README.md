# Supernova
The goal is to develop a small game engine that can render 2D and 3D scenes using Vulkan. This also includes systems for glTF scenes, physics, entities, and input.

The renderer is based on [Vulkan C++ examples and demos](https://github.com/SaschaWillems/Vulkan) by Sascha Willems.

Currently using Vulkan [1.3](https://registry.khronos.org/vulkan/specs/latest/html/vkspec.html#versions-1.3) with [Dynamic Rendering](https://registry.khronos.org/vulkan/specs/latest/html/vkspec.html#_render_pass_objects_deprecation_via_dynamic_rendering).

Visual Studio 2022 is supported, but CMake support will be added soon.

## Dependencies
The following third-party dependencies are being used.
| Dependency  | Version |
| ------------- | ------------- |
| GLFW  | 3.4 |
| GLM  | 1.0.1  |
| Dear ImGui  | 1.92.3  |
| TinyGLTF  | 2.9.6  |
| EnTT  | 3.15.0  |
| KTX  | 4.4.0  |
| stb_image  | 1.16  |
