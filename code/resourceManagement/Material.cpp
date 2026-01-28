#include "Material.h"
#include "PConstantType.h.slang"

namespace Play
{
FixedMaterial::FixedMaterial() : Material("Fixed Material") {}

FixedMaterial::~FixedMaterial() {}
FixedMaterial* FixedMaterial::Create()
{
    static ShaderID      vertexShaderID     = ShaderManager::Instance().getShaderIdByName(BuiltinShaders::BUILTIN_DEFAULT_GBUFFER_VERT_SHADER_NAME);
    static ShaderID      fragShaderID       = ShaderManager::Instance().getShaderIdByName(BuiltinShaders::BUILTIN_DEFAULT_GBUFFER_FRAG_SHADER_NAME);
    static PlayProgram*  commonFixedProgram = ProgramPool::Instance().alloc<RenderProgram>(vertexShaderID, fragShaderID);
    static FixedMaterial fixedMaterial      = {};
    commonFixedProgram->getDescriptorSetManager().addConstantRange<SceneConstant>();
    fixedMaterial._program = commonFixedProgram;
    return &fixedMaterial;
}
} // namespace Play