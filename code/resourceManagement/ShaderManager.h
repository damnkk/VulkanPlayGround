#ifndef SHADER_MANAGER_H
#define SHADER_MANAGER_H
#include <string.h>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <unordered_map>
#include "nvh/nvprint.hpp"
#include "shaderc/shaderc.h"

namespace Play {

class ShaderManager {
public:
    ShaderManager();
    ~ShaderManager();

    bool CompileShader(const std::string& source, shaderc_shader_kind kind, std::string& out_spirv);
    void SetCompilerOptions(const std::unordered_map<std::string, std::string>& options);

private:
    shaderc_compiler_t compiler_;
    shaderc_compile_options_t options_;
};
ShaderManager::ShaderManager() {
    compiler_ = shaderc_compiler_initialize();
    options_ = shaderc_compile_options_initialize();
}
} // namespace Play

#endif // SHADER_MANAGER_H