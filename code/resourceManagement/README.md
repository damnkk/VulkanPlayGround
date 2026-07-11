# Resource Management Layout

`resourceManagement` is organized by responsibility. Existing public type names are retained so this layout change does not alter runtime behavior.

- `assets/`: CPU-side asset loading and asset packages.
  - `image/`: image decoding and third-party image implementations.
  - `model/`: model import configuration, loading, and `ModelAsset` packages.
  - `AssetLoadingServer.*`: asynchronous asset-load request coordination.
- `scene/`: scene lifetime and scene representations.
  - `cpu/`: editable CPU scene graph and components.
  - `gpu/`: GPU scene mirrors; `GaussianScene.*` is the former `PlayScene.*` implementation.
  - `SceneManager.*`: coordinates the CPU graph, asset loading, and the selected GPU scene.
- `vulkan/`: Vulkan-facing resource and pipeline infrastructure.
  - `resources/`: texture/buffer wrappers, allocation, and Vulkan resource utilities.
  - `descriptors/`: descriptor layout, allocation, and cache management.
  - `pipeline/`: shaders, material bindings, pipelines, and render-pass implementations.
  - `cache/`: framebuffer and render-pass caches.
  - `legacy/`: disabled historical `VulkanDriver.cpp` implementation, retained without changing its behavior.
- `renderGraph/`: render dependency graph (RDG) nodes, resources, barriers, and builders.

Cross-module includes use paths rooted at `resourceManagement/`. The top-level `resourceManagement` directory is intentionally not exported as a CMake include directory, preventing new code from depending on the old flat layout.
