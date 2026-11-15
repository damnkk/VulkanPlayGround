# RDG 纹理池与显存对比

`ResourcePool` 替代原先按名字保存纹理的 `ResourceCache`。Blackboard 仍然只负责本帧逻辑节点的索引。

- `compile()` 扫描所有声明的 texture 读写，记录首次和末次使用，并生成各 Pass 的归还列表。
- 首次使用前按完整规格查询空闲物理纹理；Pass 的回调、end rendering 和 final layout 转换结束后，归还末次使用的纹理。
- 同一 Pass 的输入输出不能因为规格相同而共用纹理；跨越多个 Pass 的资源在最后一次访问之前始终占用物理纹理。
- 物理纹理保留 layout 和 graphics access。新逻辑资源可以丢弃旧内容，但 barrier 仍同步前任的 GPU 访问。
- Imported 纹理不进入图内回收。TAA history、Volume 累积结果等需要跨帧保留内容的纹理由对应 Pass 持有并导入。
- 目前精确匹配 extent、format、type、usage、aspect、mip、layer、samples，支持原有构造器能创建的单层 2D/3D target。Buffer 仍沿用按名字跨帧保留，尚未做使用区间复用。
- 池会跨帧保留已创建的纹理，resize 清空。工作负载缩小后不会立即释放闲置条目；不会把不同 VkImage 绑定到同一内存区间。

## 对照运行

手动编译后，分别从新进程启动，加载相同场景，保持相同窗口分辨率、TAA 设置和相机。先预热到 `created=0`，不要在两次对比中混入不同的 resize/功能切换过程。

```powershell
# 基线：仍跨帧保留资源，但禁用帧内纹理复用
.\_bin\Debug\VulkanPlayGround.exe --rendermode defer --rdg-pool-stats --rdg-no-texture-reuse

# 优化：默认允许末次使用后的纹理复用
.\_bin\Debug\VulkanPlayGround.exe --rendermode defer --rdg-pool-stats
```

统计只在分配或工作负载计数变化时输出，不逐帧刷日志：

- `logical`：本帧实际用到的池内逻辑纹理数及内存需求总和，每个逻辑纹理计算一次。
- `physical / reserved`：池中所有物理纹理数及其内存需求总和，包括当前闲置条目。
- `peak active`：本帧同时占用的池内纹理内存需求峰值。基线模式一直保留到图结束。
- `created`：本帧新创建的物理纹理数；稳定场景预热后应为 0。

字节数来自 `vkGetImageMemoryRequirements().size`，是 image 内存需求，包含驱动布局要求；不包含外部纹理、buffer、材质贴图、swapchain 或分配器空闲内存。逻辑纹理可以减少，显存收益取决于规格匹配和存活区间，不能预先保证比例。

对比两次运行稳定日志中的 `reserved`：

```text
池内节省 MiB = 基线 reserved - 复用 reserved
池内节省比例 = 池内节省 / 基线 reserved
```

进一步核对整个应用的显存，可以看 VMA 的 `allocationBytes` 和 `blockBytes`（参见 [VMA Statistics](https://gpuopen-librariesandsdks.github.io/VulkanMemoryAllocator/html/statistics.html)）。前者是资源分配量，后者是向 Vulkan 申请的内存块量；资源变少后，分配器仍可能保留空闲块。因此任务管理器中的进程专用 GPU 内存不一定按池内收益同比下降。本次没有额外接入全进程 VMA 统计。

## 手动验证

另加 `--validation` 跑 defer 和 volume，检查画面和同步错误，尤其覆盖：

1. Pass 1 写、Pass 8 读的纹理不能在 Pass 2 后被重用。
2. Pass 1 写、Pass 2 最后读取的纹理，可以供 Pass 3 及以后规格相同的资源使用。
3. 同一 Pass 同时读 A、写 B，即使规格相同也不能共用物理纹理。
4. 重复 binding 引用同一纹理只归还一次。
5. TAA 开关、Volume 静止累积与移动重置、resize 后的资源重建。

资源必须声明在 Pass 使用列表中。回调中未声明的纹理访问不在生命周期分析范围内。当前方案依赖单 graphics queue 执行，未实现 async compute 的跨队列生命周期调度。
