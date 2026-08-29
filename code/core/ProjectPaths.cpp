#include "ProjectPaths.h"

#include <nvutils/logger.hpp>
#include <nvutils/file_operations.hpp>

namespace Play
{

std::string           ProjectInfo::_projectName;
std::filesystem::path ProjectInfo::_projectPath;

namespace
{
std::filesystem::path normalizePath(const std::filesystem::path& path)
{
    std::error_code             errorCode;
    const std::filesystem::path weakPath = std::filesystem::weakly_canonical(path, errorCode);
    if (!errorCode)
    {
        return weakPath.lexically_normal();
    }

    const std::filesystem::path absolutePath = std::filesystem::absolute(path, errorCode);
    return errorCode ? path.lexically_normal() : absolutePath.lexically_normal();
}

bool isPathInsideDirectory(const std::filesystem::path& path, const std::filesystem::path& directory)
{
    std::error_code             errorCode;
    const std::filesystem::path relativePath = std::filesystem::relative(normalizePath(path), normalizePath(directory), errorCode);
    if (errorCode || relativePath.empty())
    {
        return false;
    }

    auto iter = relativePath.begin();
    return iter == relativePath.end() || *iter != "..";
}

std::filesystem::path makeImportedResourcePath(const std::filesystem::path& sourcePath, const std::filesystem::path& resourceDirectory, const GUID& uid)
{
    std::filesystem::path assetDirectory = ProjectInfo::getProjectPath() / "assets";
    if (!resourceDirectory.empty())
    {
        assetDirectory /= resourceDirectory;
    }

    std::filesystem::path targetPath = assetDirectory / sourcePath.filename();

    std::error_code errorCode;
    if (!std::filesystem::exists(targetPath, errorCode))
    {
        return targetPath;
    }

    std::string suffix = uuids::to_string(uid);
    if (suffix.empty())
    {
        suffix = "imported";
    }

    targetPath = assetDirectory / (sourcePath.stem().string() + "_" + suffix + sourcePath.extension().string());
    return targetPath;
}
} // namespace

bool ProjectInfo::setProjectPath(const std::filesystem::path& projectPath)
{
    if (projectPath.empty())
    {
        clear();
        return false;
    }

    std::filesystem::path projectDirectory = projectPath;
    if (projectDirectory.extension() == ".project")
    {
        projectDirectory = projectDirectory.parent_path();
    }

    std::error_code             errorCode;
    const std::filesystem::path absolutePath = std::filesystem::absolute(projectDirectory, errorCode);
    if (!errorCode)
    {
        projectDirectory = absolutePath;
    }

    projectDirectory              = projectDirectory.lexically_normal();
    const std::string projectName = projectDirectory.filename().string();
    if (projectDirectory.empty() || projectName.empty())
    {
        clear();
        return false;
    }

    _projectPath = std::move(projectDirectory);
    _projectName = projectName;
    return true;
}

void ProjectInfo::clear()
{
    _projectName.clear();
    _projectPath.clear();
}

bool ProjectInfo::isOpen()
{
    return !_projectPath.empty();
}

const std::string& ProjectInfo::getProjectName()
{
    return _projectName;
}

const std::filesystem::path& ProjectInfo::getProjectPath()
{
    return _projectPath;
}

std::filesystem::path ProjectInfo::getAssetMapPath()
{
    return _projectPath.empty() ? std::filesystem::path{} : _projectPath / "assetmap.json";
}

bool ProjectInfo::ensureFilePathInProject(std::string& filePath, const std::filesystem::path& resourceDirectory, const GUID& uid)
{
    if (filePath.empty())
    {
        return true;
    }

    if (!isOpen())
    {
        LOGW("Cannot import asset file {%s} without an open project\n", filePath.c_str());
        return false;
    }

    std::filesystem::path sourcePath(filePath);
    if (!sourcePath.is_absolute())
    {
        sourcePath = getProjectPath() / sourcePath;
    }
    sourcePath = normalizePath(sourcePath);

    if (isPathInsideDirectory(sourcePath, getProjectPath()))
    {
        filePath = sourcePath.string();
        return true;
    }

    std::error_code errorCode;
    if (!std::filesystem::exists(sourcePath, errorCode) || errorCode)
    {
        LOGW("Cannot import missing asset file {%s}\n", sourcePath.string().c_str());
        return false;
    }

    if (!std::filesystem::is_regular_file(sourcePath, errorCode) || errorCode)
    {
        LOGW("Cannot import non-file asset path {%s}\n", sourcePath.string().c_str());
        return false;
    }

    const std::filesystem::path targetPath = makeImportedResourcePath(sourcePath, resourceDirectory, uid);
    std::filesystem::create_directories(targetPath.parent_path(), errorCode);
    if (errorCode)
    {
        LOGW("Failed to create imported asset directory {%s}: %s\n", targetPath.parent_path().string().c_str(), errorCode.message().c_str());
        return false;
    }

    std::filesystem::copy_file(sourcePath, targetPath, std::filesystem::copy_options::none, errorCode);
    if (errorCode)
    {
        LOGW("Failed to copy imported asset file {%s} to {%s}: %s\n",
             sourcePath.string().c_str(),
             targetPath.string().c_str(),
             errorCode.message().c_str());
        return false;
    }

    filePath = normalizePath(targetPath).string();
    return true;
}
} // namespace Play
