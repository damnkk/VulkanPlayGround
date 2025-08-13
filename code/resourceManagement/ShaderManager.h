#ifndef SHADER_MANAGER_H
#define SHADER_MANAGER_H
#include <string.h>
#include <unordered_set>
#include <unordered_map>
#include "nvvk/shadermodulemanager_vk.hpp"
namespace nvvk{
    class Context;
}
namespace Play {
class ShaderManager {
public:
    static void initialize(nvvk::Context& context);
    static nvvk::ShaderModuleID registeShader(uint32_t shaderType, const std::string& shaderCodePath, const std::string& entryPoint,const std::string& prepend);
    static nvvk::ShaderModuleManager& Instance();
private:
    static void registeShader();
    static bool needsRecompilation(std::string shaderPath,uint32_t shaderType);
    static std::unordered_set<std::string> parseIncludeDependencies(const std::string& shaderPath,bool recursive);
    static nvvk::ShaderModuleManager _shaderModuleManager;
};
} // namespace Play

#endif // SHADER_MANAGER_H