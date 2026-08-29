#ifndef PROJECT_PATHS_H
#define PROJECT_PATHS_H

#include "core/Uuid.h"
#include <filesystem>

namespace Play
{

class ProjectInfo
{
public:
    static bool setProjectPath(const std::filesystem::path& projectPath);
    static void clear();

    static bool isOpen();

    static const std::string&           getProjectName();
    static const std::filesystem::path& getProjectPath();
    static std::filesystem::path getAssetMapPath();
    static bool                  ensureFilePathInProject(std::string& filePath, const std::filesystem::path& resourceDirectory, const GUID& uid);

private:
    static std::string           _projectName;
    static std::filesystem::path _projectPath;
};
} // namespace Play

#endif // PROJECT_PATHS_H
