# Changelog
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.0.0] - 2026-06-10
### Added
- Core decoupled Entity-Component-System (ECS) rendering architecture.
- `RRL::IO` module for automated Prefab, Material, and Image loading.
- Native `RRL::Data` Garbage Collection with `CASCADE_DELETE` policies.
- Hardware-accelerated OpenGL backend with embedded and runtime shader support.
- Software Rasterizer (SWR) backend with fallback scalar math (SIMD by default).
- Headless, GLFW, and OpenCV window presentation layers.

### Changed
- Decoupled `MaterialData` from `MeshData` to allow independent multi-material instantiating.

### Fixed
- Merged submesh indexing logic to perfectly map `MeshLinkage` material arrays.