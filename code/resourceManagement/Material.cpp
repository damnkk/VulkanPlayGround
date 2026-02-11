#include "Material.h"
#include "PConstantType.h.slang"
#include "GBufferConfig.h"

namespace Play
{
FixedMaterial::FixedMaterial() : Material("Fixed Material") {}

FixedMaterial::~FixedMaterial() {}
FixedMaterial* FixedMaterial::Create()
{
    static ShaderID       vertexShaderID     = ShaderManager::Instance().getShaderIdByName(BuiltinShaders::BUILTIN_DEFAULT_GBUFFER_VERT_SHADER_NAME);
    static ShaderID       fragShaderID       = ShaderManager::Instance().getShaderIdByName(BuiltinShaders::BUILTIN_DEFAULT_GBUFFER_FRAG_SHADER_NAME);
    static RenderProgram* commonFixedProgram = ProgramPool::Instance().alloc<RenderProgram>(vertexShaderID, fragShaderID);
    static FixedMaterial  fixedMaterial      = {};
    commonFixedProgram->getDescriptorSetManager().initPushConstant<GBufferPushConstant>();
    commonFixedProgram->psoState().colorWriteMasks.resize(
        GBufferConfig::RT_COUNT - 1, VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT);
    commonFixedProgram->psoState().colorBlendEnables.resize(GBufferConfig::RT_COUNT - 1, VK_FALSE);
    commonFixedProgram->psoState().colorBlendEquations.resize(GBufferConfig::RT_COUNT - 1, {
                                                                                               .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
                                                                                               .dstColorBlendFactor = VK_BLEND_FACTOR_ZERO,
                                                                                               .colorBlendOp        = VK_BLEND_OP_ADD,
                                                                                               .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
                                                                                               .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
                                                                                               .alphaBlendOp        = VK_BLEND_OP_ADD,
                                                                                           });
    commonFixedProgram->psoState().rasterizationState.cullMode = VK_CULL_MODE_BACK_BIT;
    fixedMaterial._program                                     = commonFixedProgram;
    return &fixedMaterial;
}
} // namespace Play