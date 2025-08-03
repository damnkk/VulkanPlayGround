#include "RDGPasses.hpp"

namespace Play::RDG{

    
inline RDGPass::RDGPass(std::shared_ptr<RDGShaderParameters> shaderParameters, uint8_t passType, std::optional<uint32_t> passID, std::string name)
{
    NV_ASSERT(shaderParameters);
    _shaderParameters = std::move(shaderParameters);
    _passType = static_cast<PassType>(passType);
    _name = std::move(name);
    _passID = passID;
}

void RDGPass::prepareResource(){

}
}