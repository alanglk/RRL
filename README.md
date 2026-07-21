
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
- Segmentation fault on OpenGL backend shutdown (render targets destroy)
- ImageAsset row padding. OpenCV and IplImages usually add row padding to make the image memory aligned.
- ImageAsset avoid data-copying? (I dont know if it is possible).