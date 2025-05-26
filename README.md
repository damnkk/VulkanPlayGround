# VulkanPlayGround(WIP)

A comprehensive Vulkan-based rendering engine featuring advanced graphics techniques and modern GPU programming paradigms.

## Features

### Ray Tracing
- **Multiple Importance Sampling (MIS)** - Advanced Monte Carlo integration for realistic lighting
- **PBR Materials** - Physically-based rendering with metallic-roughness workflow

![Ray Tracing Demo](/asset/raytracing.png)

### Volume Rendering
- **Medical Data Visualization** - Compute shader-based volume rendering pipeline
- **High-performance GPU acceleration** for large medical datasets

![Volume Rendering](/asset/volumeRendering.png)

### Modern Vulkan Techniques
- **Variable Rate Shading (VRS)** - Adaptive rendering quality optimization
- **Bindless Resources** - Efficient GPU resource management
- **Dynamic Uniform Buffers** - Flexible per-object data streaming

![VRS Comparison](/asset/shadingRate.png)

### Architecture
- **Data-Oriented Scene Graph** - Cache-friendly scene management
- **Modular Renderer Design** - Extensible rendering pipeline architecture

![Architecture Diagram](/asset/sceneGraph.png)

## Technical Stack
- **API**: Vulkan 1.3+
- **Language**: C++17
- **Shaders**: GLSL 4.6
- **Extensions**: VK_KHR_ray_tracing_pipeline, VK_KHR_fragment_shading_rate

## Applications
- Real-time ray-traced rendering
- Medical imaging and visualization
- High-performance graphics research
- Modern GPU architecture exploration
