# 逐帧 FrameGraph 与渲染线程

主线程处理 SDL 窗口和输入，收集 Qt 编辑器的命令、导入和保存请求，并将值类型的 `FrameRequest` 交给渲染线程。Qt 保持自己的 GUI 线程，仍通过命令和快照与运行时通信。

渲染线程运行期间拥有可变的 Scene 和 GPU Scene，按顺序执行编辑命令、场景更新、RDG、Vulkan 提交和 Present。当前的组件更新会直接改 GPU Scene 的表，录制上传时还会读这些表，所以这一版保留单一所有者。以后若场景模拟变重，可以把独立的模拟放回主线程，再交给渲染线程一份描述变更的数据，类似 UE 的 Render Proxy。

```text
主线程    发布 N ── 准备 N+1 ── 等待 N ── 发布 N+1 ── 准备 N+2
渲染线程           └─ N：场景/RDG/提交/Present ─┘     └─ N+1：…
GPU                               └─ 执行 N，可与 CPU 的 N+1 重叠
```

CPU 交接只有一个槽。主线程可提前准备一帧，只有渲染线程完成上一份请求才会交接，输入不会在 CPU 队列中积成多帧。等待期间主线程仍泵 Windows 消息。这里的完成是 CPU 上的 Present 调用返回；GPU 帧槽由现有 Vulkan timeline 管理，实际显示延迟仍受 GPU、swapchain 和显示节奏影响。Resize 也通过帧请求交接，由渲染线程重建 swapchain。退出先排空请求并 join，再销毁 Vulkan 和窗口。

这对应 UE 的两条思路：[渲染线程与帧同步](https://dev.epicgames.com/documentation/unreal-engine/parallel-rendering-overview-for-unreal-engine?lang=en-US)控制 Game Thread 能领先多少，[RDG](https://dev.epicgames.com/documentation/unreal-engine/render-dependency-graph-in-unreal-engine)把 pass 声明与底层资源分开。本项目当前以一个 CPU 渲染线程和一条 graphics queue 为基础；compute pass 仍在该 queue 录制。UE 的独立 RHI 线程、并行录制和 async compute 是后续独立的选择。

## 图和物理资源

- `BasePass::init()` 创建持久的 pass 状态；`build()` 只声明本帧用到的 pass、资源和回调。比如关闭 TAA 时，两条 TAA pass 不会进入图。
- `RDGBuilder` 创建在栈上；节点和回调只对当前帧有效。执行顺序就是声明顺序，compile 整理 descriptor bindings。原来的 DAG 遍历最终也按声明顺序执行，本版省去了那次遍历。CPU 容器仍有少量堆分配，后续是否用 arena 看实测。
- Renderer keeps a ResourcePool of physical textures by specification. RDG returns each texture after its last declared use so later logical textures with disjoint lifetimes can reuse it; buffers still use the existing name-based cache. Resize clears viewport resources. Persistent history (including Volume accumulation) belongs to its pass and is imported each frame, as is the current swapchain image. Physical layout/access records survive graphs; GPU resource destruction remains deferred. CPU-written camera and volume uniform data use frame slots. See [RDGTexturePool.md](RDGTexturePool.md) for statistics and comparison options.
- Vulkan barrier 由资源使用声明驱动，按整张图像或整个 buffer 同步。在同一 queue 上，可用 barrier 同步前一个提交中对同一资源的读写；复用不需要每帧 `vkDeviceWaitIdle`。参见 [Vulkan 同步示例](https://docs.vulkan.org/guide/latest/synchronization_examples.html)。

## 预期收益与测法

设 `M` 为可以在主线程独立执行的工作，`R` 为渲染线程上的 CPU 工作。两者串行约为 `M + R`，流水线填满后理想的 CPU 帧间隔约为 `max(M, R) + 交接成本`，最终还要受 GPU 和显示限制。下面的数值用来解释上限：

| 假设 | 串行 → 双线程的理想帧间隔 | 含义 |
|---|---:|---|
| 主线程 0.5 ms，渲染 CPU 3 ms，GPU 8 ms | 8 → 8 ms | GPU 已满。收益主要是窗口能在等待期间继续处理消息。 |
| 主线程 0.5 ms，渲染 CPU 3 ms，GPU 2 ms | 3.5 → 3 ms | CPU 理想提高约 17% 吞吐量，实际还要扣掉交接和调度。 |
| 主线程 4 ms，渲染 CPU 6 ms，GPU 5 ms | 10 → 6 ms | 两边都有足够工作时，才有接近 1.67 倍的吞吐量空间。 |

当前主线程主要是 SDL 和命令收集，场景模拟与 GPU Scene 上传仍在渲染侧。空场景的 FPS 很可能变化不大；线程唤醒与逐帧 RDG 构建还可能让极轻场景略慢。逐帧 RDG 的直接收益是可以按实际设置增减 pass。若长耗时来自录制、Present 或 GPU，主线程领先幅度会被限制，重场景的 FPS 仍取决于瓶颈。

默认启用渲染线程。用 `--single-threaded-rendering` 在同一线程运行完全相同的帧路径，可隔离线程拆分的影响。若还想知道静态图改逐帧图本身的净成本，另外用改动前版本做基线。固定项目、相机、尺寸和 VSync，预热后在 Nsight Systems 查看：

- `Main Prepare Frame`、`Main Wait for Render Thread` 与 `Render Frame` 是否重叠，等待实际花在哪一侧。
- `RDG Build`、`RDG Compile` 的中位数和 P95；`RDG Allocate Texture/Buffer` 应出现在首次需要资源、规格变化或 resize 后，而不应在稳定画面每帧出现。
- `Render Wait for GPU Slot`、`Acquire Swapchain Image`、`Present` 和 GPU 帧耗时。长等待和长 GPU 帧是不同的问题。

手动验证建议：用 `--validation` 跑 defer 和 volume；检查相机移动、TAA 历史与 volume 累积、连续 resize、最小化/恢复、编辑/导入/保存和退出。随后用单线程开关重复同一场景，对比画面与时序。
