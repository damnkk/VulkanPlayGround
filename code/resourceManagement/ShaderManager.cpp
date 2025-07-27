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

const size_t SHADER_HASH_SEED = 0xDEADBEEF;
const std::string SHADER_SAVE_DIR = "./spv";
const std::vector<std::string> SHADER_INCLUDE_DIR = {
    "./shaders",
    "./shaders/volumeRender",
    "./shaders/VRS",
    "./External/nvpro_core/nvvkhl/shaders",
};

namespace Play{

     shaderc_compiler_t ShaderManager::compiler_;
     shaderc_compile_options_t ShaderManager::options_;
     std::unordered_map<size_t,ShaderInfo> ShaderManager::_shaderMap;
    void ShaderManager::initialize(){
        compiler_ = shaderc_compiler_initialize();
        options_ = shaderc_compile_options_initialize();
        
        // Set Vulkan 1.4 target environment
        shaderc_compile_options_set_target_env(options_, shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_3);
        shaderc_compile_options_set_target_spirv(options_, shaderc_spirv_version_1_6);
        
        // Enable debug info in debug mode
        #ifdef _DEBUG
        shaderc_compile_options_set_generate_debug_info(options_);
        #endif
        
        registerShader();
        CompileShader();
    }

    void ShaderManager::registerShader(){
        registerShader("RayGenShader", "./shaders/raygen.rgen", "main", ShaderType::sRayGen);
    }

    void ShaderManager::registerShader(const std::string& ShaderName, const std::string& ShaderCodePath,const std::string& shaderEntryPoint, ShaderType shaderType) {
        size_t shaderHash = SHADER_HASH_SEED;
        nvh::hashCombine(shaderHash, ShaderName, shaderType);
        if(_shaderMap.find(shaderHash) != _shaderMap.end()) {
            throw std::runtime_error("Shader already registered");
        }

        ShaderInfo info;
        info.type = shaderType;
        info.codePath = ShaderCodePath;
        info.shaderName = ShaderName;
        info.spvPath = SHADER_SAVE_DIR + "/" + ShaderName + ".spv";
        info.entryPoint = shaderEntryPoint;
        _shaderMap[shaderHash] = info;
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
    bool ShaderManager::needsRecompilation(const ShaderInfo& info) {
        std::string spvFilePath = SHADER_SAVE_DIR + "/" + info.shaderName + ".spv";
        
        // 1. 检查spv文件是否存在
        if (!std::filesystem::exists(spvFilePath)) {
            return true;
        }

        auto spvModifyTime = getFileModifyTime(spvFilePath);
        auto shaderModifyTime = getFileModifyTime(info.codePath);

        // 2. 检查shader源文件是否有修改
        if (shaderModifyTime > spvModifyTime) {
            return true;
        }

        // 3. 检查include依赖文件是否有修改
        try {
            auto dependencies = parseIncludeDependencies(info.codePath);
            for (const auto& dep : dependencies) {
                auto depModifyTime = getFileModifyTime(dep);
                if (depModifyTime > spvModifyTime) {
                    return true;
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "Error parsing dependencies for " << info.shaderName << ": " << e.what() << std::endl;
            return true; // 出错时重新编译
        }

        return false;
    }

    // 编译单个shader
    bool ShaderManager::CompileShader(const std::string& shaderPath, ShaderType type, std::string& spirvData) {
        // 读取shader源码
        std::ifstream file(shaderPath, std::ios::ate | std::ios::binary);
        if (!file.is_open()) {
            std::cerr << "Failed to open shader file: " << shaderPath << std::endl;
            return false;
        }

        size_t fileSize = static_cast<size_t>(file.tellg());
        std::string shaderCode(fileSize, '\0');
        file.seekg(0);
        file.read(shaderCode.data(), fileSize);
        file.close();
        auto includeResolveFunc = [](void* user_data, const char* requested_source, int type,
        const char* requesting_source, size_t include_depth)->shaderc_include_result*{
            std::string includePath;
            
            // 尝试在各个include目录中查找文件
            for (const auto& dir : SHADER_INCLUDE_DIR) {
            std::string candidate = dir + "/" + requested_source;
            if (std::filesystem::exists(candidate)) {
                includePath = candidate;
                break;
            }
            }
            
            if (includePath.empty()) {
            // 如果没找到，尝试相对于请求文件的路径
            std::filesystem::path requestingPath(requesting_source);
            std::filesystem::path relativePath = requestingPath.parent_path() / requested_source;
            if (std::filesystem::exists(relativePath)) {
                includePath = relativePath.string();
            }
            }
            
            // 如果还是没找到，返回错误
            if (includePath.empty()) {
            auto result = new shaderc_include_result;
            std::string* errorMsg = new std::string("Cannot find include file: " + std::string(requested_source));
            result->source_name = "";
            result->source_name_length = 0;
            result->content = errorMsg->c_str();
            result->content_length = errorMsg->length();
            result->user_data = errorMsg;
            return result;
            }
            
            // 读取文件内容
            std::ifstream file(includePath, std::ios::binary);
            if (!file.is_open()) {
            auto result = new shaderc_include_result;
            std::string* errorMsg = new std::string("Cannot open include file: " + includePath);
            result->source_name = "";
            result->source_name_length = 0;
            result->content = errorMsg->c_str();
            result->content_length = errorMsg->length();
            result->user_data = errorMsg;
            return result;
            }
            
            std::string* content = new std::string((std::istreambuf_iterator<char>(file)),
                               std::istreambuf_iterator<char>());
            std::string* sourceName = new std::string(includePath);
            
            auto result = new shaderc_include_result;
            result->source_name = sourceName->c_str();
            result->source_name_length = sourceName->length();
            result->content = content->c_str();
            result->content_length = content->length();
            result->user_data = new std::pair<std::string*, std::string*>(sourceName, content);
            
            return result;
        };

        auto includeResultRelease = [](void* user_data, shaderc_include_result* include_result)->void{
            if (include_result) {
            if (include_result->user_data) {
                auto* pair = static_cast<std::pair<std::string*, std::string*>*>(include_result->user_data);
                delete pair->first;  // source name
                delete pair->second; // content
                delete pair;
            } else {
                // 错误情况下，user_data可能直接是错误消息字符串
                auto* errorMsg = static_cast<std::string*>(include_result->user_data);
                delete errorMsg;
            }
            delete include_result;
            }
        };

        // 创建临时编译选项并设置include路径
        shaderc_compile_options_t tempOptions = shaderc_compile_options_clone(options_);
        shaderc_compile_options_set_include_callbacks(tempOptions, includeResolveFunc, includeResultRelease, nullptr);
       

        // 确定shader类型
        shaderc_shader_kind shaderKind;
        switch (type) {
            case ShaderType::sVertex: shaderKind = shaderc_vertex_shader; break;
            case ShaderType::sFragment: shaderKind = shaderc_fragment_shader; break;
            case ShaderType::sCompute: shaderKind = shaderc_compute_shader; break;
            case ShaderType::sMesh: shaderKind = shaderc_mesh_shader; break;
            case ShaderType::sRayGen: shaderKind = shaderc_raygen_shader; break;
            case ShaderType::sAnyHit: shaderKind = shaderc_anyhit_shader; break;
            case ShaderType::sClosestHit: shaderKind = shaderc_closesthit_shader; break;
            case ShaderType::sMiss: shaderKind = shaderc_miss_shader; break;
            default:
                std::cerr << "Unknown shader type" << std::endl;
                shaderc_compile_options_release(tempOptions);
                return false;
        }

        // 编译shader
        shaderc_compilation_result_t result = shaderc_compile_into_spv(
            ShaderManager::compiler_, shaderCode.c_str(), shaderCode.size(), 
            shaderKind, shaderPath.c_str(), "main", tempOptions);

        shaderc_compile_options_release(tempOptions);

        if (shaderc_result_get_compilation_status(result) != shaderc_compilation_status_success) {
            std::cerr << "Shader compilation failed: " << shaderc_result_get_error_message(result) << std::endl;
            shaderc_result_release(result);
            return false;
        }
        if(shaderc_result_get_num_warnings(result)>0)
        LOGW("Shader compilation succeeded with warnings:\n %s", shaderc_result_get_error_message(result));
        // 获取编译结果
        const char* spirv = shaderc_result_get_bytes(result);
        size_t spirvSize = shaderc_result_get_length(result);
        spirvData.assign(spirv, spirv + spirvSize);

        shaderc_result_release(result);
        return true;
    }

    bool ShaderManager::CompileShader(){
        // 确保输出目录存在
        std::filesystem::create_directories(SHADER_SAVE_DIR);

        for (auto& [hash, info] : _shaderMap) {
            // 检查是否需要重新编译
            if (!needsRecompilation(info)) continue;

            // 需要重新编译
            std::cout << "Compiling shader: " << info.shaderName << std::endl;
            std::string spirv;
            if (!CompileShader(info.codePath, info.type, spirv)) {
                std::cerr << "Failed to compile shader: " << info.shaderName << std::endl;
                return false;
            }

            // 保存编译结果到文件
            std::string spvFilePath = SHADER_SAVE_DIR + "/" + info.shaderName + ".spv";
            std::ofstream spvFile(spvFilePath, std::ios::binary);
            if (spvFile.is_open()) {
                spvFile.write(spirv.data(), spirv.size());
                spvFile.close();
            } else {
                std::cerr << "Failed to save compiled shader: " << spvFilePath << std::endl;
            }
        }
        return true;
    }

    const ShaderInfo* ShaderManager::GetShaderWithType(std::string ShaderName, ShaderType type){
        auto hash = SHADER_HASH_SEED;
        nvh::hashCombine(hash, ShaderName, type);
        auto it = _shaderMap.find(hash);
        if (it != _shaderMap.end()) {
            if(it->second.spvData.empty()){
                it->second.spvData = nvh::loadFile(it->second.spvData, true);
            }
            return &it->second;
        }
        return nullptr;
    }

} // namespace Play