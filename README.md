
[![Language](https://img.shields.io/badge/Language-C%2B%2B17-blue.svg)](https://en.cppreference.com/w/cpp/17)
[![Build System](https://img.shields.io/badge/Build%20System-CMake%203.15%2B-orange.svg)](https://cmake.org/)
[![License](https://img.shields.io/badge/License-GPL2-green.svg)](LICENSE)
[![Tests](https://img.shields.io/github/actions/workflow/status/alanglk/RRL/ci.yml?label=tests)](https://github.com/alanglk/RRL/actions/workflows/ci.yml)
[![codecov](https://codecov.io/gh/alanglk/RRL/graph/badge.svg)](https://codecov.io/gh/alanglk/RRL)

Robotics Rendering Library (RRL) is a C++ library designed to create real-time robotics scene representations with ease.


# Description
The main objective of RRL is to leverage a highly performant Entity Component System (EnTT) to create a fast, flexible, and dynamic rendering architecture.
It enables users to set up complex 2D or 3D scenes easily while maintaining a performant execution.

The core idea of the library is the strict separation between scene data and the Rendering Hardware Interface (RHI). 
This decoupling makes it possible to hot-swap rendering strategies at runtime. For example, a user can switch 
from a GLFW-windowed OpenGL renderer to an OpenCV headless software renderer mid-execution.

The goal behind this architecture is to provide a flexible API capable of supporting a wide variety of robotics projects: 
from rendering a NuScenes dataset visualization to acting as the visual environment for a Reinforcement Learning engine.
- Core: Handles entities, data updates (textures, materials and meshes), and the 3D spatial transform tree.
- The RHI Module: Implements backend-specific systems (e.g., OpenGL, CUDA kernels, OpenCV, Software Renderers, or WebGPU/WebAssembly).
- The Presentation Layer: The final output (e.g., a GLFW window or an OpenCV image matrix) is handled natively by the active RHI backend.

The RHI provides an API to load and swap render targets at runtime (future versions will add support for user defined RHIs). 
Platform-incompatible targets are safely filtered out at compile-time via the CMake build system.


# Library Conventions
To ensure scenes are produced and transformed coherently across the entire pipeline, this library enforces strict geometrical and mathematical conventions. 
All matrix operations are implemented using GLM.

## Mathematical Foundations
- Homogeneous Coordinates: All points, lines, and matrices use homogeneous coordinates. For example, 3D points are represented as 4×1 column vectors ($[x, y, z, w]^T$).
- Multiplication Order: Right-multiplication (column-major convention) is used for transformations. Transforming a point $P$ by a matrix $M$ is evaluated as: $P' = M \cdot P$.
- Orientation: All systems strictly utilize a right-handed coordinate system.
- Rotations: The Transformation (tf) module supports both standard Euler/matrix rotations and Quaternion rotations natively.

## Coordinate Systems
| System | Name | Convention / Axis Mapping | Description |
| :--- | :--- | :--- | :--- |
| **LCS** | Local Coordinate System | **X-front, Y-left, Z-up** | Follows ISO 8855 standards for local object space. |
| **CCS** | Camera Coordinate System | <unknown> | Internally follows a standard OpenGL conventions, but users can set other camera convention. The CameraSystem automatically maps the ISO 8855 world to this space during the View Matrix calculation. |
| **WCS** | World Coordinate System | Static global anchor | The global reference frame. It shares the same ISO 8855 axes as the LCS. |



# Library Design

> Insert overall diagram


# Todo
- ImageAsset row padding. OpenCV and IplImages usually add row padding to make the image memory aligned.
- ImageAsset avoid data-copying? (I dont know if it is possible).


Current limitations:
- Rendering background elements not supported (HDRI maps, camera background textures...)
- Cameras are fixed to output just one color texture (what happens with deph, infra-red, semantic segmentation...).
- The RenderFrame pipeline is fixed and implemented on each RHI backend. Backends should provide necessary functions for execution, but the render pipeline should be general and shared across all backends.
- The is no support for multi-camera frame stitching.

Fixing approach:
Implement a [Render Graph](https://logins.github.io/graphics/2021/05/31/RenderGraphs.html) inside the RHI module (Render Direct Graph - RDG submodule). RRL will provide a default render graph pipeline but the user can still define its own render layer steps. The RDG module will be responsible of calling RHIBackend for resource allocation and resource destruction and each RHIBackend implementation (OpenGL, Software, WebGPU) will implement their specific RenderPass functions to finally execute them by calling rhi::rdg::Execute.


Step 1. Upgrade the RRL base source code:
- Implement Multiple Render Targets (MRT): RHIBackend should receive a RenderTargetDescriptor on CreateRenderTarget. Instead of hardcoding texture allocations inside the FBO creation, the descriptor will take a list of already-allocated TextureHandles (colors and depth).
> Note on TARGET_MAIN: This remains a special reserved handle (0). The RDG will treat it as a "Retained Resource" (the window) that cannot be created or destroyed.

- Decouple Cameras from Targets: Currently, CameraComponent owns a target_fbo. This will be phased out. Cameras should only define what they see (culling_mask) and how they see it (Projection). The RDG Passes will define where the image goes.

- Enable GPU-Only Transient Allocations: To enable frame-to-frame rendering, intermediate textures, and Multiple Render Targets (MRT), we must bypass the CPU AssetManager. We will add a CreateRenderTexture(width, height, format, ...) function directly to the RHIBackend. This allocates pure, empty VRAM and returns a TextureHandle for the RDG to use.

- Support Texture Arrays for Stitching: Update the new CreateRenderTexture to accept an array_layers parameter. This allows the backend to allocate a GL_TEXTURE_2D_ARRAY, enabling multiple cameras to render to different slices of the same FBO simultaneously.

- Complete the SceneEnvironment setup: Finalize the SceneEnvironment structures and implement the public SceneModule facade methods so users can configure the skybox/background. This data will be stored in the SceneCache for the RDG's "Background Pass" to consume.