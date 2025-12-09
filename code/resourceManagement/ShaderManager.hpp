#ifndef SHADERMANAGER_H
#define SHADERMANAGER_H
#include "nvslang/slang.hpp"
#include "nvvkglsl/glsl.hpp"
#include "nvutils/file_mapping.hpp"
#include "PlayAllocator.h"
#include "spirv_reflect.h"

namespace Play
{
const uint32_t MaxShaderModules = 1024;
enum class ShaderStage : uint32_t
{
    eVertex = 0,
    eFragment,
    eCompute,
    eRayGen,
    eRayAnyHit,
    eRayClosestHit,
    eRayMiss,
    eRayIntersection,
    eRayCallable,
    eRayTask,
    eRayMesh,
    eCount
};

enum class ShaderType
{
    eGLSL,
    eSLANG,
    eHLSL,
    eCount
};

VkDescriptorType      spvToDescriptorType(SpvReflectDescriptorType type);
VkPipelineStageFlags2 spvToVkStageFlags(SpvReflectShaderStageFlagBits flags);

class PlayElement;
struct ShaderModule
{
    ShaderModule(uint32_t id) : _poolId(id) {}

    VkShaderModule       _shaderModule;
    uint32_t             _poolId;
    ShaderType           _type;
    std::vector<uint8_t> _spvCode;
    std::string          _name;
};

class ShaderPool : public BasePool<ShaderModule>
{
public:
    ShaderModule* alloc();
    void          free(uint32_t id);
    ShaderModule* get(uint32_t id)
    {
        if (id >= _objs.size() || id < 0 || _objs[id] == nullptr)
        {
            return nullptr;
        }
        return _objs[id];
    }
};

class ShaderManager
{
public:
    static ShaderManager& Instance();
    ShaderManager() = default;
    void init();

    void registBuiltInShader();
    void addSearchPath(const std::filesystem::path& path);

    uint32_t loadShaderFromFile(std::string name, const std::filesystem::path& filePath, ShaderStage stage, ShaderType type = ShaderType::eGLSL,
                                std::string entry = "main");
    void     eraseShaderByName(std::string name);
    void     eraseShaderById(uint32_t id);
    void     eraseShaderByModule(const ShaderModule& module);
    const ShaderModule* getShaderById(uint32_t id);
    const ShaderModule* getShaderByName(std::string name);
    uint32_t            getShaderIdByName(std::string name);

    void deInit();

protected:
    bool checkShaderUpdate(std::filesystem::path shaderPath, std::filesystem::path spvPath);

private:
    ShaderPool                                _shaderPool;
    nvslang::SlangCompiler                    _slangCompiler;
    nvvkglsl::GlslCompiler                    _glslCCompiler;
    std::unordered_map<std::string, uint32_t> _nameIdMap;
    std::vector<std::filesystem::path>        _searchPaths;
};

} // namespace Play

#endif // SHADERMANAGER_H