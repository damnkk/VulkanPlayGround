#include "Asset.h"
#include "core/ProjectPaths.h"
#include "nvutils/logger.hpp"

namespace Play
{
namespace
{
std::filesystem::path normalizePath(const std::filesystem::path& path)
{
    std::error_code             errorCode;
    const std::filesystem::path normalizedPath = std::filesystem::weakly_canonical(path, errorCode);
    return errorCode ? path.lexically_normal() : normalizedPath.lexically_normal();
}

bool isPathInsideDirectory(const std::filesystem::path& path, const std::filesystem::path& directory)
{
    std::error_code             errorCode;
    const std::filesystem::path relativePath = std::filesystem::relative(normalizePath(path), normalizePath(directory), errorCode);
    if (errorCode)
    {
        return false;
    }

    const auto iter = relativePath.begin();
    return iter == relativePath.end() || *iter != "..";
}
} // namespace

bool Asset::onSaveAsset()
{
    return true;
}

void Asset::onLoadAsset()
{
    if (!_filePath.empty())
    {
        const std::filesystem::path sourcePath = normalizePath(_filePath);
        if (!isPathInsideDirectory(sourcePath, ProjectInfo::getProjectPath()))
        {
            const std::filesystem::path targetPath =
                ProjectInfo::getProjectPath() / "assets" / getAssetTypeName() / (_name + sourcePath.extension().string());

            std::error_code errorCode;
            std::filesystem::copy_file(sourcePath, targetPath, std::filesystem::copy_options::overwrite_existing, errorCode);
            if (errorCode)
            {
                LOGW("Failed to copy asset resource {%s} to {%s}: %s\n",
                     sourcePath.string().c_str(),
                     targetPath.string().c_str(),
                     errorCode.message().c_str());
                throw std::filesystem::filesystem_error("Failed to copy asset resource", sourcePath, targetPath, errorCode);
            }
            _filePath = normalizePath(targetPath).string();
        }
    }
}

} // namespace Play
