# Pipeline state preparation

`DrawCommand` describes a drawable and its material reference. It does not wrap RHI commands or own a pass-specific PSO. A pass consumes that reference while preparing pipeline state.

The preparation boundary is inside the pass execution callback, after RDG has prepared descriptors and the current RenderPass:

1. Obtain a pass-local `PipelineStateBuilder` from `RenderContext`.
2. Supply the pass's shader/state defaults and, optionally, the batch's `MaterialInstance`.
3. Receive the completed initializer by value; the defaults stay unchanged.
4. Bind the completed initializer, then submit the draw or dispatch.

The builder resolves attachment formats/count and samples from RenderPass, applies material shaders and state, resolves material descriptors, and creates the pipeline layout. Material references stop at `build()`; `bindPipeline()` and the pipeline cache accept only const initializers.

`build()` returns a completed initializer, rather than a success flag plus partially prepared output. Missing graphics pass context or invalid material descriptor resources throw an error during preparation. `bindPipeline()` returns void: it gets or creates the actual PSO and binds it. The cache asserts the resolved-layout contract and checks Vulkan creation results with `NVVK_CHECK`, matching the existing fail-fast handling used for pipeline layouts. Pass callbacks do not silently skip their draw or dispatch on creation failure.

## Passes with fixed shaders

```cpp
const auto pipeline = context.createPipelineStateBuilder().build(_lightPassPipeline);
context.bindPipeline(pipeline);
// Bind push constants and draw.
```

The same sequence accepts `ComputePipelineStateInitializer`, without render targets or material raster/blend state.

## Material batches

For a pass consuming material batches, prepare once per batch before issuing its indirect draws:

```cpp
auto builder = context.createPipelineStateBuilder();
for (const auto& batch : batches)
{
    const auto pipeline = builder.build(_gbufferPipeline, batch.materialInstanceRef);
    context.bindPipeline(pipeline);
    // Submit the batch's indirect draws using draw IDs and GPU Scene data.
}
```

This is the intended consumption point for existing `DrawBatch`/GPU Scene material buckets. GBuffer's execute callback is currently empty; this change does not implement its draw submission.

Material stores blend policy and color write masks, not attachment count. `GraphicsPipelineStateInitializer::setRenderTargetState()` sizes all blend arrays to the current pass, including zero color attachments, and synchronizes sample count. Material application fills those prepared arrays.

Pass members remain defaults. A local copy prevents one material or pass from modifying defaults used by subsequent preparation. Keep builders within their current pass callback; RDG refreshes scene/frame/pass descriptors for each pass execution.

The existing Custom Material descriptor set and pipeline layout slots remain in use. Preparation changes their ownership stage, not their binding scheme.
