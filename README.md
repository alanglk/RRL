# RRL
[![Language](https://img.shields.io/badge/Language-C%2B%2B17-blue.svg)](https://en.cppreference.com/w/cpp/17)
[![Build System](https://img.shields.io/badge/Build%20System-CMake%203.15%2B-orange.svg)](https://cmake.org/)
[![License](https://img.shields.io/badge/License-GPL2-green.svg)](LICENSE)
[![Tests](https://img.shields.io/github/actions/workflow/status/alanglk/RRL/ci.yml?label=tests)](https://github.com/alanglk/RRL/actions/workflows/ci.yml)
[![codecov](https://codecov.io/gh/alanglk/RRL/graph/badge.svg)](https://codecov.io/gh/alanglk/RRL)

Robotic Rendering Library (RRL) is a C++ synchronous rendering library designed to create real-time robotics scenes representations.



## Description
The main objective of RRL is to leverage an Entity Component System (EnTT) to create a fast, flexible, and dynamic rendering architecture. It enables users to set up complex 2D or 3D scenes with ease while maintaining high-performance execution.

The library is designed around four core pillars:

- **Transformation Management**: Manages static and dynamic entity transformations (e.g., fixed world objects, moving cameras). It implements a Scene Transform Tree for high-performance updates and easy lookup, backed by dedicated systems that keep the transform hierarchy synchronized.

- **Dependency Tracking**: Features a basic garbage collector that utilizes a Scene Dependency Graph. Because entities can be dynamically created, destroyed, or updated (e.g., dynamic LOD), and components may depend on one another, this graph ensures safe entity removal and updates without breaking references.

- **Flexible Public API**: Provides a clean, highly performant API for managing entities, components, and runtime systems. To maximize performance, RRL avoids virtual methods and interfaces. Instead, it uses a compile-time/static approach for system management. Users can easily inject custom components, entities, and runtime systems alongside built-in systems (Transform, Garbage Collection, Camera Matrix Computing).

- **Decoupled RHI (Rendering Hardware Interface)**: Completely decouples the core scene logic from the rendering backend. The RHI module implements specific components, entities, and systems to target different backends (e.g., OpenGL, CUDA kernels, OpenCV, Software Renderers, or WebGPU/WebAssembly). The output (e.g., a GLFW window or an OpenCV image) is handled natively by the specific RHI module.

The RHI provides an API to define, register, unregister, and hot-swap render targets at runtime. Platform-incompatible targets are filtered out at compile-time via the CMake build system.



# Library Conventions

To ensure scenes are produced and transformed coherently across the entire pipeline, this library enforces strict geometrical and mathematical conventions. All matrix operations are implemented using **GLM**.

## Mathematical Foundations

* **Homogeneous Coordinates**: All points, lines, and matrices use homogeneous coordinates. For example, 3D points are represented as $4 \times 1$ column vectors ($[x, y, z, w]^T$).
* **Multiplication Order**: **Right-multiplication** (column-major convention) is used for transformations. Transforming a point $P$ by a matrix $M$ is evaluated as:
    $$P' = M \cdot P$$
* **Orientation**: All systems strictly utilize a **right-handed coordinate system**.
* **Rotations**: The `tf` (transformation) module supports both basic matrix rotations and **quaternion rotations** (for smooth interpolations).


## Coordinate Systems

The library manages interactions between five distinct coordinate system specifications:

| System | Name | Convention / Axis Mapping | Description |
| :--- | :--- | :--- | :--- |
| **LCS** | Local Coordinate System | **X-front, Y-left, Z-up** | Follows ISO 8855 standards for local object space. |
| **CCS** | Camera Coordinate System | **X-right, Y-down, Z-front** | Follows standard OpenCV conventions. |
| **WCS** | World Coordinate System | Static global anchor | The global reference frame (typically initialized to match the first spawned LCS). |
| **ICS** | Image Coordinate System | **X-right, Y-down** | The 2D image plane itself (pixel space). |
| **GCS** | Geographic Coordinate System | Geographic / UTM | Real-world global coordinates (see implementation details below). |

## Runtime Transformations & Extensibility

> [!NOTE] 
> Transformations ($tf$) can be updated dynamically at runtime. 

The `tf` module provides a highly generalized API designed to support future ecosystem wrappers, including:
1.  **Key-frame Based Systems**: Systems that compute transformations for discrete frames, requiring robust pose and rotation interpolation (e.g., SLERP for quaternions) between frames.
2.  **Geographic Extensions**: Scalable integrations utilizing the Geographic Coordinate System (GCS).

## Hardware Interface (RHI) Mapping

Because our core Camera Coordinate System (CCS) uses a **Y-down, Z-forward** convention, transformations must adapt to the Normalized Device Coordinates (NDC) of the target rendering backend:

### 🟨 WebGPU
* **NDC**: X-right, Y-up, Z-forward ($0$ to $1$)
* **Integration Fit**: **Moderate**
* **Handling**: Requires flipping the **Y-axis** within the projection matrix before passing data to the pipeline.

### 🟦 OpenGL
* **NDC**: X-right, Y-up, Z-backward ($-1$ to $1$)
* **Integration Fit**: **Poor**
* **Handling**: Standard OpenGL view space expects the camera to look down the negative Z-axis with Y pointing up. Because our CCS looks down positive Z with Y down, the OpenGL projection matrix must **flip both the Y and Z axes** to map correctly to OpenGL's clip space.

### 🟩 CUDA
* **NDC / Space**: Custom defined per application context.



## Library Structure

```text
RRL/
├── CMakeLists.txt           # Root build architecture configuration
├── CMakePresets.json        # Build profiles (dev, test, release orchestration)
├── include/                 # Public Headers
│   └── RRL/
│       ├── core/            # High-performance ECS wrappers & data types
│       ├── tf/              # Kinematic transform tree & linear algebra
│       ├── camera/          # Sensor matrix utilities (OpenCV model math)
│       ├── gb/              # Graph garbage collectors & dependency monitors
│       └── rhi/             # Rendering Hardware Interface base systems
├── src/                     # Source Implementations
│   ├── include/
│   │   └── RRL/
│   │       └── geo/         # Private, localized geospatial structures
│   ├── core/
│   ├── tf/
│   ├── geo/
│   └── rhi/
│       ├── opengl/          # Conditionally compiled hardware targets
│       ├── webgpu/          # Conditionally compiled hardware targets
│       └── cuda/            # Conditionally compiled hardware targets
├── third_party/             # Native vendor configurations (EnTT, GLM)
├── tests/                   # Unit Testing suite (GoogleTest framework)
└── examples/                # Quick-start setup modules and reference implementations
```
