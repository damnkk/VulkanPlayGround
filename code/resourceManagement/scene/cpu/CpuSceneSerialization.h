#ifndef CPU_SCENE_SERIALIZATION_H
#define CPU_SCENE_SERIALIZATION_H

#include "resourceManagement/scene/cpu/CpuScene.h"

namespace Play
{

// The project asset table deliberately describes only persisted binary payloads.
// Import settings belong to the import step that creates the .bin file, not to a project.
enum class ProjectAssetType : uint32_t
{
    eModel
};

struct ProjectAssetRecord
{
    std::string guid;
    // Persist relative paths here so moving the project directory does not invalidate assets.
    std::string      binaryPath;
    ProjectAssetType type = ProjectAssetType::eModel;
};

class ProjectAssetTable
{
public:
    bool set(const ProjectAssetRecord& record);
    void clear();

    const ProjectAssetRecord* find(const std::string& guid) const;

    const std::vector<ProjectAssetRecord>& getRecords() const
    {
        return _records;
    }

private:
    std::vector<ProjectAssetRecord> _records;
};

// A .project path identifies a directory package containing the scene and asset JSON documents.
bool createProjectArchive(const std::string& projectPath, std::string* errorMessage = nullptr);

bool saveProjectArchive(const CpuScene& scene, const ProjectAssetTable& assets, const std::string& projectPath, std::string* errorMessage = nullptr);

// Reads a project package, validates every model GUID against its asset table, and only then
// replaces the destination CPU scene and project asset table. No binary assets are loaded here.
bool loadProjectArchive(CpuScene& scene, ProjectAssetTable& assets, const std::string& projectPath, std::string* errorMessage = nullptr);

} // namespace Play

#endif // CPU_SCENE_SERIALIZATION_H
