# GPUScene 注册、更新与渲染入口

当前实现负责模型/纹理注册、持久的 draw/instance 快照、脏区间上传，以及 GPU 剔除需要的候选列表和输出缓冲区。`GBufferPass` 的执行体原本为空，这次没有替它选择 Custom Material pipeline 或实现剔除算法。CPU 不生成每帧可见 draw 参数：每帧 count 清零后，需要 compute 写入 indirect 参数、visibleDrawIDs 和 count，绘制才会产生输出。

## 生命周期

1. `StaticMeshComponent::onLoad()` 在 `LoadAssetBind(Model, _model)` 后调用 `registerModel()`；运行时 `setModel()` 走同一注册入口。
2. `GpuScene::registerModel()` 持有 `ModelRef`，同一模型只注册一次，返回稳定的 `modelID`。模型与纹理区间保留到这个 GPUScene 销毁，组件删除只回收 draw/instance 区间。
3. 注册时把模型纹理追加到 GPUScene 的纹理列表，保持模型内纹理索引不变。起始位置写入 GPUScene 模型表副本的 `buffers.textureOffset`；原始 Model 的地址表不携带特定场景的索引。
4. 组件在注册时调用 `GpuScene::updateDrawCommands()` 缓存 draw，换材质时更新缓存，删除时通过 `releaseDraws()` 注销。`Component::onSceneChanged()` 在组件或实体的加入、移除及销毁时通知组件，外部保留 shared_ptr 也不会阻止 draw 注销。脱离场景的组件只注册模型，重新加入场景时恢复 draw 注册。
5. `VulkanRuntime::prepareFrame()` 等待当前 frame slot 完成。之后 `Renderer::OnPreRender()` → `SceneManager::update()` → scene tick → `GpuScene::prepareFrame()`，只同步该 slot 的脏区间。scene tick 不再提交或收集 draw。
6. RDG 的首个 `GpuSceneUpload` pass 在 graphics queue 上调用 `cmdUpload()`，有脏数据时记录 buffer copy 和可见性屏障，并在 GPU 上清零当前帧的可见 count，然后才执行其他 pass。

模型资源和纹理图像跨帧共享。动态数据按实际 `getFrameCycleSize()` 分配：每个 frame slot 一个 device-local arena 和一个 scene descriptor set。arena 内按 16 字节边界划分 root、models、draws、instances、candidates、buckets、indirect、counts、visibleDrawIDs 区域。

GPUScene 不自行创建 staging buffer。`prepareFrame()` 准备当前 slot 的容量、root 和桶信息；`cmdUpload()` 将脏区间交给 `PlayResourceManager::appendBuffer()`，随后在同一次调用中通过 `cmdUploadAppended()` 记录复制。临时 staging 关联本次 RDG 提交的 frame timeline 值，后续通过 `releaseStaging()` 回收已经完成的上传。RDG 的 timeline signal 覆盖 `ALL_COMMANDS`，包含 transfer；重建 frame slot 时在销毁旧 semaphore 前回收 staging。场景修改应在 `prepareFrame()` 前完成。

索引、顶点流、模型材质、纹理元数据和 drawable 元数据保留在 Model 的不可变 GPU buffer 中。绘制使用 `vkCmdDrawIndirectCount`，由 vertex shader 按 device address 读取索引和顶点；不创建、复制或绑定共享 index buffer。

## CPU 更新入口

```cpp
auto mesh = entity->addComponent<Play::StaticMeshComponent>();
mesh->setModel(model); // Model 已完成 GPU 上传，成功后 getModelID() 有效。
mesh->setInstanceCount(100);
mesh->updateInstanceRange(20, 5, [](size_t index, Play::InstanceData& instance)
{
    instance.transform = /* 相对于组件的模型实例变换 */ glm::mat4(1.0f);
    instance.customData = glm::vec4(float(index), 0, 0, 0);
});
```

默认有一个单位变换实例。`setInstanceCount()` 设置实际数量，`reserveInstances()` 只预留 CPU 容量，`addInstance()` 追加实例；要从空集合开始追加，先 `setInstanceCount(0)`。实例数组目前是运行时数据，没有加入组件序列化。

`setInstanceData()` 更新单个元素，`updateInstanceRange(first, count, callback)` 更新连续范围，`updateInstanceState()` 更新全部。回调中的 index 是组件内的绝对实例索引。每个组件的所有 drawable 共用一个实例区间，不复制各 submesh 的矩阵。上传的 `worldFromModel = worldFromComponent * instance.transform`，组件或祖先变换变化会标记该组件的全部实例。

CPU 修改会标记每个 frame slot 的脏区间。每个 slot 上传后只清除自己的标记；同一张表在该 slot 上的多次修改合并为一个覆盖区间。实例区间按二次幂扩容，迁移时更新该组件所有 draw 的 `instanceBase`。空闲 CPU 区间可以复用，因为 GPU 每个 slot 持有独立快照。

GPUScene 持久保存 draw 注册表、候选列表和材质桶。只有 draw 增删或材质桶归属改变时，`rebuildDrawSnapshot()` 才重建一次候选/桶快照，并分别标记各 frame slot 待同步。实例数量变为零仍保留候选记录，由 compute 处理；数量或实例基址变化只更新 draw POD，实例变换变化只更新实例表。首次同步完成后，静止场景不会重新分桶、复制候选列表或上传 root/draw/instance。

可见 count、indirect 参数、visibleDrawIDs 属于每帧 GPU 输出，与上述持久输入分开。CPU 只预留输出容量，不上传这些输出的默认绘制内容；每帧 GPU 清零 count 后，compute 根据当前相机重新产生结果。输出缓冲区旧内容无需整体清空，有效前缀由新的 count 限定。

## 根地址与 shader 跳转

CPU 根入口：

```cpp
const auto& gpuScene = vkDriver->getSceneManager()->getGpuScene();
VkDeviceAddress sceneAddress = gpuScene->getRootAddress(); // 本帧 prepareFrame() 之后获取。
```

不要跨帧缓存这个地址；它属于当前 frame slot，容量增长后也可能改变。共享结构定义在 `shaders/Hdevice.h`，Slang 访问函数在 `shaders/GpuScene.slang`。

```text
sceneAddress → GpuSceneData
  ├─ bucketAddress[bucketID].firstCommand + DrawIndex
  │    → visibleDrawIDAddress[slot] → drawID
  │    → drawAddress[drawID] → { modelID, drawableID, instanceBase, instanceCount }
  ├─ modelAddress[modelID] → GpuSceneModelData
  │    ├─ drawableAddress[drawableID] → { meshIndex, modelFromMesh, bounds, materialIndex }
  │    ├─ buffers.meshInfoAddress[meshIndex] → ModelMeshInfo
  │    │    ├─ indexAddress[VertexID] → mesh 内的 vertexIndex
  │    │    └─ vertexStreamAddress → ModelVertexStreamInfo → position/normal/UV/... 地址[vertexIndex]
  │    ├─ buffers.materialAddress[materialIndex] → GltfShadeMaterial
  │    └─ buffers.textureInfoAddress[material texture-info index] → ModelTextureInfo
  │         → buffers.textureOffset + textureIndex → set 1 / binding 3 纹理数组
  └─ instanceAddress[InstanceIndex] → { worldFromModel, modelFromWorld, customData }
```

`DrawIndex` 是当前 indirect 调用内的局部索引。`InstanceIndex` 使用 Vulkan/SPIR-V 语义，已包含 indirect 参数的 `firstInstance = instanceBase`；如果 shader 使用从零开始的局部实例编号，则需要自行加 `instanceBase`。

顶点访问示意（`drawIndex`、`vertexID`、`instanceIndex` 分别指 Vulkan 的 DrawIndex、VertexIndex、InstanceIndex built-in）：

```cpp
GpuSceneData scene = loadGpuScene(sceneAddress);
uint drawID = loadGpuSceneDrawID(scene, bucketID, drawIndex);
GpuSceneDrawData draw = loadGpuSceneDraw(scene, drawID);
GpuSceneModelData model = loadGpuSceneModel(scene, draw.modelID);
ModelDrawableInfo drawable = loadGpuSceneDrawable(model, draw.drawableID);
ModelMeshInfo mesh = loadGpuSceneMesh(model, drawable);
ModelVertexStreamInfo streams = loadGpuSceneVertexStreams(mesh);
GpuSceneInstanceData instance = loadGpuSceneInstance(scene, instanceIndex);
uint vertexIndex = loadGpuSceneVertexIndex(mesh, vertexID);
float3 position = ((float3*) streams.positionAddress)[vertexIndex];
```

indirect 参数的 `vertexCount = drawable.indexCount`、`firstVertex = 0`，因此 `vertexID` 是从零开始的索引条目编号。`mesh.indexAddress` 在模型上传时已经计算为 `model.buffers.indexAddress + drawable.firstIndex * sizeof(uint)`，从该地址读取时不要再次加 `firstIndex`。等价的模型级读法是 `((uint*) model.buffers.indexAddress)[drawable.firstIndex + vertexID]`。读出的 `vertexIndex` 相对于 mesh 的顶点流；`streams.positionAddress` 等地址也已包含 mesh 的 `firstVertex` 偏移。

将 `position` 经 `drawable.modelFromMesh`、`instance.worldFromModel` 变换后进入相机空间，注意 shader 的矩阵存储/乘法约定与 GLM 保持一致。`modelFromWorld` 可配合 drawable 的变换构造法线变换。

fragment 阶段如果需要模型材质，vertex 阶段可通过 `nointerpolation` 传递 `drawID`，再调用 `loadGpuSceneDraw()` 和 `loadGpuSceneModel()`。纹理访问先 `loadGpuSceneTextureInfo()`，成功后再 `sampleGpuSceneTexture()`。后者已应用模型的 `textureOffset`、UV 通道/变换和 `NonUniformResourceIndex`。纹理采样仍需要绑定全局 sampler set 与 scene texture set；device address 负责定位场景数据和纹理索引，不替代 Vulkan 的采样描述符。

## Indirect 入口

`GpuScene::getDrawBuckets()` 返回当前帧的桶；初版按同一个 `MaterialInstance*` 分桶，空指针对应渲染 pass 的默认材质。调用者应保证这个材质引用的生命周期，并为当前 pass 选择兼容 pipeline/layout；不同 pass 可以采用自己的过滤与分桶策略。

对每个桶，先绑定 pipeline、scene descriptor set 和该桶材质，将 `sceneAddress`、`bucketID` 放入对应 pipeline 的 push constant，然后调用：

```cpp
gpuScene->cmdDrawBucket(cmd, bucketID);
```

这个方法执行 `vkCmdDrawIndirectCount`，由 shader 完成 index/vertex pulling。也可自行调用 Vulkan：`getFrameBuffer()->buffer` 同时作为 indirect buffer 和 count buffer，`getIndirectOffset(bucketID)`、`getCountOffset(bucketID)` 返回**字节偏移**；bucket 的 `firstCommand`、`countIndex` 是**元素偏移**。

`GpuSceneIndirectDrawCommand` 与 `VkDrawIndirectCommand` 都是 16 字节，字段依次为 `{ vertexCount, instanceCount, firstVertex, firstInstance }`，compute 应写入 `{ drawable.indexCount, draw.instanceCount, 0, draw.instanceBase }`。现有 DefaultGbuffer vertex shader 的手动 index pulling 与此一致；接入 root 后保留这一步索引读取。Custom Material 的参数依然由已有材质绑定系统提供，没有自动打包进 GPUScene root。

接 GPU 剔除时，首个 `GpuSceneUpload` pass 已调用 `cmdResetDrawCounts(cmd)`；在其后的同一 graphics queue 上、render pass 外 dispatch，再调用 `cmdBarrierAfterCulling(cmd)`。compute 遍历持久的 `candidateAddress[0..candidateCount)`，每个 candidate 带 `drawID` 与 `bucketID`，跳过 `instanceCount == 0` 的记录，对可见者执行：

```text
localSlot = atomicAdd(counts[bucket.countIndex], 1)
slot = bucket.firstCommand + localSlot
indirect[slot] = { drawable.indexCount, draw.instanceCount, 0, draw.instanceBase }
visibleDrawIDs[slot] = candidate.drawID
```

每个 candidate 最多输出一个 draw，桶容量等于该桶候选数。输出顺序不保证稳定；需要距离顺序时另加排序。这里预留的是 draw 级剔除接口，逐实例压紧还需要独立的 visible-instance 映射；跨队列异步 compute 也需另接 semaphore/所有权同步。
