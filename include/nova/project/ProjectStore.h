#pragma once

#include "nova/project/Project.h"

#include <string>
#include <vector>

namespace nova::project {

// Recent projects, autosave, rolling backups, and optional version restore.
class ProjectStore {
public:
    static std::vector<std::string> recentProjects(int maxCount = 10);
    static void addRecentProject(const std::string& path);
    static void removeRecentProject(const std::string& path);

    static bool autosave(const Project& project, std::string* error = nullptr);
    static bool createBackup(const Project& project, std::string* error = nullptr);

    struct VersionEntry {
        std::string path;
        std::string label;
    };
    static std::vector<VersionEntry> versionHistory(const std::string& projectPath);
    static std::optional<Project> restoreVersion(const std::string& backupPath,
                                                 std::string* error = nullptr);

    static std::string autosavePathFor(const std::string& projectPath);
    static std::string backupDirectoryFor(const std::string& projectPath);
};

} // namespace nova::project
