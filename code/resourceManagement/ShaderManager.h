#ifndef SHADER_MANAGER_H
#define SHADER_MANAGER_H
#include <string.h>
#include <unordered_set>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <unordered_map>
#include "nvh/nvprint.hpp"
#include "shaderc/shaderc.h"

namespace Play {

    enum struct ShaderType{
        sVertex,
        sFragment,
        sCompute,
        sMesh,
        sRayGen,
        sAnyHit,
        sClosestHit,
        sMiss
    };
struct ShaderInfo{
    ShaderType type;
    std::string shaderName;
    std::string codePath;
    std::string spvPath;
    std::string entryPoint;
    std::string spvData;
};
class ShaderManager {
public:
    static void initialize();
    static bool CompileShader();
    static void SetCompilerOptions(const std::unordered_map<std::string, std::string>& options);
    static const ShaderInfo* GetShaderWithType(std::string ShaderName, ShaderType type);

private:
    static std::unordered_set<std::string> parseIncludeDependencies(const std::string& shaderPath, bool recursive = true);
    static bool needsRecompilation(const ShaderInfo& info);
    static bool CompileShader(const std::string& shaderPath, ShaderType type, std::string& spirvData);
    static void registerShader();
    static void registerShader(const std::string& ShaderName, const std::string& ShaderCodePath,const std::string& shaderEntryPoint, ShaderType shaderType);
    static shaderc_compiler_t compiler_;
    static shaderc_compile_options_t options_;
    static std::unordered_map<size_t,ShaderInfo> _shaderMap;
};
} // namespace Play

#endif // SHADER_MANAGER_H