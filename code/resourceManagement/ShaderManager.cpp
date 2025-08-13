#include "ShaderManager.h"
#include "utils.hpp"
#include <filesystem>
#include <fstream>
#include <regex>
#include <iostream>
#include <queue>
#include <unordered_set>
#include <nvh/fileoperations.hpp>
#include <nvh/container_utils.hpp>
#include "nvvk/context_vk.hpp"
#include <nvp/NvFoundation.h>

const size_t SHADER_HASH_SEED = 0xDEADBEEF;
const std::string SHADER_SAVE_DIR = "./spv";
const std::vector<std::string> SHADER_INCLUDE_DIR = {
    "./shaders",
    "./shaders/volumeRender",
    "./shaders/VRS",
    "./External/nvpro_core/nvvkhl/shaders",
};

namespace Play{
    
    std::string getShaderType(uint32_t shaderType){
        switch (shaderType) {
            case VK_SHADER_STAGE_VERTEX_BIT: return "Vertex";
            case VK_SHADER_STAGE_FRAGMENT_BIT: return "Fragment";
            case VK_SHADER_STAGE_COMPUTE_BIT: return "Compute";
            case VK_SHADER_STAGE_RAYGEN_BIT_KHR: return "RayGen";
            case VK_SHADER_STAGE_ANY_HIT_BIT_KHR: return "AnyHit";
            case VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR: return "ClosestHit";
            case VK_SHADER_STAGE_MISS_BIT_KHR: return "Miss";
            case VK_SHADER_STAGE_INTERSECTION_BIT_KHR: return "Intersection";
            case VK_SHADER_STAGE_CALLABLE_BIT_KHR: return "Callable";
            case VK_SHADER_STAGE_TASK_BIT_EXT: return "Task";
            case VK_SHADER_STAGE_MESH_BIT_EXT: return "Mesh";
            // 添加其他着色器类型
            default: return "Unknown";
        }
    }

    inline std::string getSpvFileName(std::string sCodePath,uint32_t shaderType){
        std::filesystem::path shaderFilePath = sCodePath;
        std::string filename = shaderFilePath.stem().string();
        std::string spvFilePath = SHADER_SAVE_DIR + "/" + filename + getShaderType(shaderType) + ".spv";
        return spvFilePath;
    }
    
    nvvk::ShaderModuleManager ShaderManager::_shaderModuleManager;

    nvvk::ShaderModuleManager& ShaderManager::Instance(){
        return _shaderModuleManager;
    }

    void ShaderManager::initialize(nvvk::Context& context){
        _shaderModuleManager.init(context.m_device,1,3);     
        _shaderModuleManager.addDirectory("./shaders");
        _shaderModuleManager.addDirectory("./shaders/volumeRender");
        _shaderModuleManager.addDirectory("./shaders/VRS");
        _shaderModuleManager.addDirectory("./External/nvpro_core/nvvkhl/shaders");
        _shaderModuleManager.setOptimizationLevel(shaderc_optimization_level_performance);
        _shaderModuleManager.m_keepModuleSPIRV = true;

        registeShader();
        
    }

    void ShaderManager::registeShader(){
        registeShader(VK_SHADER_STAGE_RAYGEN_BIT_KHR, "./shaders/raygen.rgen", "main", "");
    }

    nvvk::ShaderModuleID ShaderManager::registeShader(uint32_t shaderType, const std::string& shaderCodePath, const std::string& entryPoint,const std::string& prepend) {
        if(shaderCodePath.ends_with(".spv")){
           return _shaderModuleManager.createShaderModule(shaderType, shaderCodePath,prepend,nvh::ShaderFileManager::FILETYPE_SPIRV,entryPoint);
        }
        if(needsRecompilation(shaderCodePath, shaderType)){
            nvvk::ShaderModuleID shaderModuleID = _shaderModuleManager.createShaderModule(shaderType, shaderCodePath);
            NV_ASSERT(shaderModuleID.isValid());
            auto& module = _shaderModuleManager.getShaderModule(shaderModuleID);
            std::filesystem::path spvFilePath = getSpvFileName(shaderCodePath, shaderType);
            std::filesystem::create_directories(spvFilePath.parent_path());
            std::ofstream spvFile(spvFilePath, std::ios::binary);
            if (spvFile.is_open()) {
                spvFile.write(reinterpret_cast<const char*>(module.moduleSPIRV.data()), 
                             module.moduleSPIRV.size() * sizeof(module.moduleSPIRV[0]));
                spvFile.close();
            }
            return shaderModuleID;

        }else{

            std::string spvFilePath = getSpvFileName(shaderCodePath, shaderType);
            return _shaderModuleManager.createShaderModule(shaderType, spvFilePath,"",nvh::ShaderFileManager::FILETYPE_SPIRV,entryPoint);
        }
        return nvvk::ShaderModuleID();
    }

    // 获取文件的最后修改时间
    std::filesystem::file_time_type getFileModifyTime(const std::string& filePath) {
        if (!std::filesystem::exists(filePath)) {
            return std::filesystem::file_time_type::min();
        }
        return std::filesystem::last_write_time(filePath);
    }

    std::unordered_set<std::string> ShaderManager::parseIncludeDependencies(const std::string& shaderPath,bool recursive){
        std::unordered_set<std::string> dependencies;
        std::ifstream file(shaderPath);
        std::string line;
        std::regex includeRegex(R"(#include\s*([<"])([^">]+)[">])");

        while (std::getline(file, line)) {
            std::smatch match;
            if (std::regex_search(line, match, includeRegex)) {
                for (const auto& dir : SHADER_INCLUDE_DIR) {
                    std::string includePath = dir + "/" + match[2].str();
                    if(std::filesystem::exists(includePath))
                    dependencies.insert(includePath);
                }
            }
        }
        if(recursive){
            std::queue<std::string> pathQueue;
            std::for_each(dependencies.begin(), dependencies.end(), [&](const std::string& dep) {
                pathQueue.push(dep);
            });

            while (!pathQueue.empty()) {
                std::string current = pathQueue.front();
                pathQueue.pop();

                auto subDeps = parseIncludeDependencies(current, true);
                std::for_each(subDeps.begin(), subDeps.end(), [&](const std::string& dep) {
                    dependencies.insert(dep);
                });
            }
        }
        return dependencies;
    }

    // 检查是否需要重新编译
    bool ShaderManager::needsRecompilation(std::string shaderPath,uint32_t shaderType) {
        std::string spvFilePath = getSpvFileName(shaderPath, shaderType);

        // 1. 检查spv文件是否存在
        if (!std::filesystem::exists(spvFilePath)) {
            return true;
        }

        auto spvModifyTime = getFileModifyTime(spvFilePath);
        auto shaderModifyTime = getFileModifyTime(shaderPath);

        // 2. 检查shader源文件是否有修改
        if (shaderModifyTime > spvModifyTime) {
            return true;
        }

        // 3. 检查include依赖文件是否有修改
        try {
            auto dependencies = parseIncludeDependencies(shaderPath,true);
            for (const auto& dep : dependencies) {
                auto depModifyTime = getFileModifyTime(dep);
                if (depModifyTime > spvModifyTime) {
                    return true;
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "Error parsing dependencies for " << shaderPath << ": " << e.what() << std::endl;
            return true; // 出错时重新编译
        }
        return false;
    }
} // namespace Play