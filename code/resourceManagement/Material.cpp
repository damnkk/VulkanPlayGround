#include "Material.h"
#include "PConstantType.h.slang"

namespace Play
{

FixedMaterial::FixedMaterial(const std::string& name) : Material(name)
{
    _program = getCommonFixedProgram();
}

PlayProgram* FixedMaterial::getCommonFixedProgram()
{
    static ShaderID     vertexShaderID     = ShaderManager::Instance().getShaderIdByName(BuiltinShaders::BUILTIN_DEFAULT_GBUFFER_VERT_SHADER_NAME);
    static ShaderID     fragShaderID       = ShaderManager::Instance().getShaderIdByName(BuiltinShaders::BUILTIN_DEFAULT_GBUFFER_FRAG_SHADER_NAME);
    static PlayProgram* commonFixedProgram = ProgramPool::Instance().alloc<RenderProgram>(vertexShaderID, fragShaderID);
    commonFixedProgram->getDescriptorSetManager().addConstantRange<SceneConstant>();
    return commonFixedProgram;
}
} // namespace Play