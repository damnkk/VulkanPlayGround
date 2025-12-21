#include "ShaderManager.hpp"
#include <regex>
#include <set>
#include "utils.hpp"
#include "VulkanDriver.h"
#include "nvvk/check_error.hpp"
#include "nvvk/debug_util.hpp"
#include "nvaftermath/aftermath.hpp"
namespace Play
{

ShaderModule* ShaderPool::alloc()
{
    std::lock_guard<std::mutex> lock(_mutex);
    assert(_availableIndex < _objs.size());
    uint32_t index = _freeIndices[_availableIndex++];
    if (_objs[index] != nullptr)
    {
        _objs[index]->_poolId = index;
        return _objs[index];
    }
    else
    {
        _objs[index] = static_cast<ShaderModule*>(new ShaderModule(index));
    }
    return _objs[index];
}

void ShaderPool::free(uint32_t id)
{
    if (id >= _objs.size())
    {
        return;
    }
    _freeIndices[--_availableIndex] = id;
    _objs[id]->_poolId              = -1;
}

void ShaderManager::init()
{
    _shaderPool.init(MaxShaderModules, &PlayResourceManager::Instance());
    std::filesystem::path shaderBasePath = std::filesystem::path(getBaseFilePath()) / "shaders";
    _searchPaths = {shaderBasePath, shaderBasePath / "newShaders", std::filesystem::path(getBaseFilePath()) / "External/nvpro_core2/nvshaders"};
    _glslCCompiler.addSearchPaths(_searchPaths);
    _glslCCompiler.defaultOptions();
    _glslCCompiler.defaultTarget();
    _glslCCompiler.options().SetGenerateDebugInfo();
    _glslCCompiler.options().SetOptimizationLevel(shaderc_optimization_level_performance);
    _glslCCompiler.options().AddMacroDefinition("GLSL");
    _slangCompiler.defaultOptions();
    _slangCompiler.defaultTarget();
    _slangCompiler.addSearchPaths(_searchPaths);
    _slangCompiler.addOption({slang::CompilerOptionName::DebugInformation, {slang::CompilerOptionValueKind::Int, SLANG_DEBUG_INFO_LEVEL_MAXIMAL}});

#if defined(AFTERMATH_AVAILABLE)
    // This aftermath callback is used to report the shader hash (Spirv) to the Aftermath library.
    _slangCompiler.setCompileCallback(
        [&](const std::filesystem::path& sourceFile, const uint32_t* spirvCode, size_t spirvSize)
        {
            std::span<const uint32_t> data(spirvCode, spirvSize / sizeof(uint32_t));
            AftermathCrashTracker::getInstance().addShaderBinary(data);
        });
#endif
    registBuiltInShader();
}

void ShaderManager::registBuiltInShader()
{
    uint32_t RayGenId2    = loadShaderFromFile("rayQuery", "test.frag", ShaderStage::eCompute, ShaderType::eGLSL, "main");
    uint32_t RayGenId     = loadShaderFromFile("slangtesttt", "slangtest.slang", ShaderStage::eFragment, ShaderType::eSLANG, "main");
    uint32_t CompGenRayId = loadShaderFromFile("volumeGenRay", "volumeRender/volumeGenRay.comp", ShaderStage::eCompute, ShaderType::eGLSL, "main");
}

VkDescriptorType spvToDescriptorType(SpvReflectDescriptorType type)
{
    switch (type)
    {
        case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
            return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER:
            return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        case SPV_REFLECT_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
            return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_IMAGE:
            return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLER:
            return VK_DESCRIPTOR_TYPE_SAMPLER;
        case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
            return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
            return VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
        case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
            return VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
        case SPV_REFLECT_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
            return VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
        case SPV_REFLECT_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:
            return VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
        default:
            return VK_DESCRIPTOR_TYPE_MAX_ENUM;
    }
}

VkPipelineStageFlags2 spvToVkStageFlags(SpvReflectShaderStageFlagBits flags)
{
    switch (flags)
    {
        case SPV_REFLECT_SHADER_STAGE_VERTEX_BIT:
            return VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT;
        case SPV_REFLECT_SHADER_STAGE_FRAGMENT_BIT:
            return VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
        case SPV_REFLECT_SHADER_STAGE_COMPUTE_BIT:
            return VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
        case SPV_REFLECT_SHADER_STAGE_RAYGEN_BIT_KHR:
            return VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR;
        case SPV_REFLECT_SHADER_STAGE_ANY_HIT_BIT_KHR:
            return VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR;
        case SPV_REFLECT_SHADER_STAGE_CLOSEST_HIT_BIT_KHR:
            return VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR;
        case SPV_REFLECT_SHADER_STAGE_MISS_BIT_KHR:
            return VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR;
        case SPV_REFLECT_SHADER_STAGE_INTERSECTION_BIT_KHR:
            return VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR;
        case SPV_REFLECT_SHADER_STAGE_CALLABLE_BIT_KHR:
            return VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR;
        case SPV_REFLECT_SHADER_STAGE_TASK_BIT_NV:
            return VK_PIPELINE_STAGE_2_TASK_SHADER_BIT_NV;
        case SPV_REFLECT_SHADER_STAGE_MESH_BIT_NV:
            return VK_PIPELINE_STAGE_2_MESH_SHADER_BIT_NV;
        default:
            return VK_PIPELINE_STAGE_2_NONE;
    }
}

void ShaderManager::addSearchPath(const std::filesystem::path& path)
{
    _searchPaths.push_back(path);
}

shaderc_shader_kind getShaderKind(ShaderStage stage)
{
    switch (stage)
    {
        case ShaderStage::eVertex:
            return shaderc_vertex_shader;
        case ShaderStage::eFragment:
            return shaderc_fragment_shader;
        case ShaderStage::eCompute:
            return shaderc_compute_shader;
        case ShaderStage::eRayGen:
            return shaderc_raygen_shader;
        case ShaderStage::eRayAnyHit:
            return shaderc_anyhit_shader;
        case ShaderStage::eRayClosestHit:
            return shaderc_closesthit_shader;
        case ShaderStage::eRayMiss:
            return shaderc_miss_shader;
        case ShaderStage::eRayIntersection:
            return shaderc_intersection_shader;
        case ShaderStage::eRayCallable:
            return shaderc_callable_shader;
        case ShaderStage::eRayTask:
            return shaderc_task_shader;
        case ShaderStage::eRayMesh:
            return shaderc_mesh_shader;
        default:
            return shaderc_glsl_infer_from_source;
    }
}

VkShaderStageFlagBits getVkShaderKind(ShaderStage stage)
{
    switch (stage)
    {
        case ShaderStage::eVertex:
            return VK_SHADER_STAGE_VERTEX_BIT;
        case ShaderStage::eFragment:
            return VK_SHADER_STAGE_FRAGMENT_BIT;
        case ShaderStage::eCompute:
            return VK_SHADER_STAGE_COMPUTE_BIT;
        case ShaderStage::eRayGen:
            return VK_SHADER_STAGE_RAYGEN_BIT_KHR;
        case ShaderStage::eRayAnyHit:
            return VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
        case ShaderStage::eRayClosestHit:
            return VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
        case ShaderStage::eRayMiss:
            return VK_SHADER_STAGE_MISS_BIT_KHR;
        case ShaderStage::eRayIntersection:
            return VK_SHADER_STAGE_INTERSECTION_BIT_KHR;
        case ShaderStage::eRayCallable:
            return VK_SHADER_STAGE_CALLABLE_BIT_KHR;
        case ShaderStage::eRayTask:
            return VK_SHADER_STAGE_TASK_BIT_NV;
        case ShaderStage::eRayMesh:
            return VK_SHADER_STAGE_MESH_BIT_NV;
        default:
            return VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;
    }
}

bool ShaderManager::checkShaderUpdate(std::filesystem::path shaderPath, std::filesystem::path spvPath)
{
    if (!std::filesystem::exists(spvPath))
    {
        return false;
    }
    auto spvTime = std::filesystem::last_write_time(spvPath);

    std::set<std::filesystem::path>    allFiles;
    std::vector<std::filesystem::path> toProcess;

    // 在_searchPaths下查找shaderPath实际存在的路径
    std::filesystem::path shaderRealPath;
    for (const auto& searchPath : _searchPaths)
    {
        auto tryPath = searchPath / shaderPath;
        if (std::filesystem::exists(tryPath))
        {
            shaderRealPath = std::filesystem::absolute(tryPath);
            break;
        }
    }
    if (shaderRealPath.empty() || !std::filesystem::exists(shaderRealPath))
    {
        // 主shader文件没找到
        return false;
    }
    toProcess.push_back(shaderRealPath);

    while (!toProcess.empty())
    {
        auto curFile = toProcess.back();
        toProcess.pop_back();

        if (allFiles.count(curFile)) continue;
        allFiles.insert(curFile);

        if (!std::filesystem::exists(curFile)) continue;

        auto fileTime = std::filesystem::last_write_time(curFile);
        if (fileTime > spvTime)
        {
            return false;
        }

        std::ifstream fin(curFile);
        std::string   line;
        std::regex    includeRegex(R"(#include\s*["<](.*)[">])");
        while (std::getline(fin, line))
        {
            std::smatch match;
            if (std::regex_search(line, match, includeRegex))
            {
                std::string incFile = match[1].str();
                if (incFile.empty()) continue;

                // 在_searchPaths下查找头文件实际存在的路径
                std::filesystem::path incRealPath;
                for (const auto& searchPath : _searchPaths)
                {
                    auto tryPath = searchPath / incFile;
                    if (std::filesystem::exists(tryPath))
                    {
                        incRealPath = std::filesystem::absolute(tryPath);
                        break;
                    }
                }
                if (!incRealPath.empty() && std::filesystem::exists(incRealPath))
                {
                    toProcess.push_back(incRealPath);
                }
            }
        }
    }
    return true;
}

uint32_t ShaderManager::loadShaderFromFile(std::string name, const std::filesystem::path& filePath, ShaderStage stage, ShaderType type,
                                           std::string entry)
{
    if (type == ShaderType::eSLANG)
    {
        if (_nameIdMap.find(name) != _nameIdMap.end())
        {
            return _nameIdMap[name];
        }
        std::filesystem::path spvPath     = getBaseFilePath() / "spv";
        std::filesystem::path spvFileName = name + ".spv";
        std::filesystem::path fullSpvPath = spvPath / spvFileName;

        if (std::filesystem::exists(fullSpvPath) && checkShaderUpdate(filePath, fullSpvPath))
        {
            // Load pre-compiled SPV file
            std::ifstream file(fullSpvPath, std::ios::binary | std::ios::ate);
            if (file.is_open())
            {
                std::streamsize size = file.tellg();
                file.seekg(0, std::ios::beg);

                std::vector<uint8_t> buffer(size);
                if (file.read(reinterpret_cast<char*>(buffer.data()), size))
                {
                    ShaderModule* module = _shaderPool.alloc();
                    module->_spvCode     = buffer;
                    module->_type        = ShaderType::eSLANG;
                    module->_name        = name;
                    module->_entryPoint  = entry;

                    VkShaderModuleCreateInfo createInfo{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
                    createInfo.codeSize = module->_spvCode.size();
                    createInfo.pCode    = reinterpret_cast<const uint32_t*>(module->_spvCode.data());

                    NVVK_CHECK(vkCreateShaderModule(vkDriver->_device, &createInfo, nullptr, &module->_shaderModule));
                    _nameIdMap[name] = module->_poolId;
                    return module->_poolId;
                }
            }
        }
        ShaderModule* module = _shaderPool.alloc();
        auto          result = _slangCompiler.compileFile(filePath);
        if (!result)
        {
            const std::string& errorMessages = _slangCompiler.getLastDiagnosticMessage();
            _shaderPool.free(module->_poolId);
            LOGE("Compilation failed: %s\n", errorMessages.c_str());
            return -1;
        }
        const std::string& warningMessages = _slangCompiler.getLastDiagnosticMessage();
        if (!warningMessages.empty())
        {
            LOGW("Compilation succeeded with warnings: %s\n", warningMessages.c_str());
        }
        module->_type = ShaderType::eSLANG;
        module->_name = name;
        module->_spvCode.resize(_slangCompiler.getSpirvSize());
        module->_entryPoint = entry;
        std::memcpy(module->_spvCode.data(), _slangCompiler.getSpirv(), _slangCompiler.getSpirvSize());
        VkShaderModuleCreateInfo createInfo{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        createInfo.codeSize = module->_spvCode.size();
        createInfo.pCode    = reinterpret_cast<const uint32_t*>(module->_spvCode.data());
        NVVK_CHECK(vkCreateShaderModule(vkDriver->_device, &createInfo, nullptr, &module->_shaderModule));
        _nameIdMap[name] = module->_poolId;
        std::ofstream spvFile(fullSpvPath, std::ios::binary);
        if (spvFile.is_open())
        {
            spvFile.write(reinterpret_cast<const char*>(createInfo.pCode), createInfo.codeSize);
            spvFile.close();
        }
        return module->_poolId;
    }
    else
    {
        if (type == ShaderType::eGLSL)
        {
            std::filesystem::path spvPath     = getBaseFilePath() / "spv";
            std::filesystem::path spvFileName = name + ".spv";
            std::filesystem::path fullSpvPath = spvPath / spvFileName;
            if (_nameIdMap.find(name) != _nameIdMap.end())
            {
                return _nameIdMap[name];
            }
            if (std::filesystem::exists(fullSpvPath) && checkShaderUpdate(filePath, fullSpvPath))
            {
                // Load pre-compiled SPV file
                std::ifstream file(fullSpvPath, std::ios::binary | std::ios::ate);
                if (file.is_open())
                {
                    std::streamsize size = file.tellg();
                    file.seekg(0, std::ios::beg);

                    std::vector<uint8_t> buffer(size);
                    if (file.read(reinterpret_cast<char*>(buffer.data()), size))
                    {
                        ShaderModule* module = _shaderPool.alloc();
                        module->_spvCode.resize(buffer.size() / sizeof(uint8_t));
                        std::memcpy(module->_spvCode.data(), buffer.data(), buffer.size());

                        module->_type       = ShaderType::eGLSL;
                        module->_name       = name;
                        module->_entryPoint = entry;

                        VkShaderModuleCreateInfo createInfo{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
                        createInfo.codeSize = module->_spvCode.size() * sizeof(uint8_t);
                        createInfo.pCode    = reinterpret_cast<const uint32_t*>(module->_spvCode.data());

                        NVVK_CHECK(vkCreateShaderModule(vkDriver->_device, &createInfo, nullptr, &module->_shaderModule));
                        _nameIdMap[name] = module->_poolId;
                        return module->_poolId;
                    }
                }
            }
            ShaderModule* module = _shaderPool.alloc();
            auto          result = _glslCCompiler.compileFile(filePath, getShaderKind(stage));
            if (result.GetNumErrors())
            {
                _shaderPool.free(module->_poolId);
                LOGE(result.GetErrorMessage().c_str());
                return -1;
            }
            module->_spvCode.resize(_glslCCompiler.getSpirvSize(result));
            std::memcpy(module->_spvCode.data(), _glslCCompiler.getSpirv(result), _glslCCompiler.getSpirvSize(result));
            module->_type                       = ShaderType::eGLSL;
            module->_name                       = name;
            module->_entryPoint                 = entry;
            VkShaderModuleCreateInfo createInfo = _glslCCompiler.makeShaderModuleCreateInfo(result, 0);
            NVVK_CHECK(vkCreateShaderModule(vkDriver->_device, &createInfo, nullptr, &module->_shaderModule));
            _nameIdMap[name] = module->_poolId;
            // Save the compiled SPV code to disk

            std::ofstream spvFile(fullSpvPath, std::ios::binary);
            if (spvFile.is_open())
            {
                spvFile.write(reinterpret_cast<const char*>(module->_spvCode.data()), module->_spvCode.size());
                spvFile.close();
            }
            return module->_poolId;
        }
        else if (type == ShaderType::eHLSL)
        {
            // TODO
        }
    }
    return ~0U;
}
void                ShaderManager::eraseShaderByName(std::string name) {}
void                ShaderManager::eraseShaderById(uint32_t id) {}
void                ShaderManager::eraseShaderByModule(const ShaderModule& module) {}
const ShaderModule* ShaderManager::getShaderById(uint32_t id)
{
    return _shaderPool.get(id);
}

const ShaderModule* ShaderManager::getShaderByName(std::string name)
{
    return _shaderPool.get(_nameIdMap[name]);
}

uint32_t ShaderManager::getShaderIdByName(std::string name)
{
    if (_nameIdMap.find(name) != _nameIdMap.end())
    {
        return _nameIdMap[name];
    }
    return ~0U;
}
void ShaderManager::deInit()
{
    for (auto& [name, id] : _nameIdMap)
    {
        ShaderModule* module = _shaderPool.get(id);
        if (module)
        {
            vkDestroyShaderModule(vkDriver->_device, module->_shaderModule, nullptr);
            _shaderPool.free(id);
        }
    }
    _nameIdMap.clear();
    _shaderPool.deinit();
}

ShaderManager& ShaderManager::Instance()
{
    static ShaderManager manager;
    return manager;
}

} // namespace Play