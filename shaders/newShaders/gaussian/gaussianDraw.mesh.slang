#include "gaussian/gaussianLib.h.slang"
[[vk::binding(0, 3)]]
RWStructuredBuffer<IndrectBuffer> indirectBuffer;

#define RASTER_MESH_WORKGROUP_SIZE 32
[numthreads(RASTER_MESH_WORKGROUP_SIZE, 1, 1)]
[outputtopology("triangle")]
[shader("mesh")]
void main(uint gtid: SV_GroupThreadID, out vertices float4 outVerts[3], out indices uint3 outPrims[1])
{
    int instanceCount = indirectBuffer[0].instanceCount;
    SetMeshOutputCounts(3, 1);

    // Single-thread mesh shader that emits one full-screen-ish triangle.
    if (gtid == 0)
    {
        outVerts[0] = float4(-1.0, -1.0, 0.0, 1.0);
        outVerts[1] = float4(3.0, -1.0, 0.0, 1.0);
        outVerts[2] = float4(-1.0, 3.0, 0.0, 1.0);
        outPrims[0] = uint3(0, 1, 2);
    }
}
