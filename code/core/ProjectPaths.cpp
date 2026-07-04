#include "ProjectPaths.h"

#include <nvutils/file_operations.hpp>

namespace Play
{
std::filesystem::path getBaseFilePath()
{
#ifdef TARGET_EXE_TO_SOURCE_DIRECTORY
    const std::filesystem::path exeDir   = nvutils::getExecutablePath().parent_path();
    const std::filesystem::path basePath = exeDir / TARGET_EXE_TO_SOURCE_DIRECTORY;
    std::error_code             ec;
    const auto                  canonicalPath = std::filesystem::weakly_canonical(basePath, ec);
    return ec ? basePath.lexically_normal() : canonicalPath;
#else
    return std::filesystem::current_path();
#endif
}
} // namespace Play
