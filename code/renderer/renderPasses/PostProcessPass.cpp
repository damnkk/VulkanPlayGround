#include "PostProcessPass.h"
#include "RDG/RDG.h"
#include "PlayProgram.h"
#include "ShaderManager.hpp"
namespace Play
{
void PostProcessPass::init()
{
    _postProgram = std::make_unique<RenderProgram>(_element->getDevice());
    _postProgram->setFragModuleID(ShaderManager::Instance().getShaderIdByName("postProcessf"))
        .setVertexModuleID(ShaderManager::Instance().getShaderIdByName("postProcessv"))
        .finish();
}

void PostProcessPass::build(RDG::RDGBuilder* rdgBuilder) {}

void PostProcessPass::deinit()
{
    _postProgram->deinit();
}

} // namespace Play