#pragma once

#include "nova/project/Project.h"

#include <optional>
#include <string>

namespace nova::project {

class ProjectIO {
public:
    static bool save(const Project& project, const std::string& path, std::string* error = nullptr);
    static std::optional<Project> load(const std::string& path, std::string* error = nullptr);
    static std::optional<Project> loadTemplate(const std::string& templatePath,
                                               std::string* error = nullptr);
};

} // namespace nova::project
