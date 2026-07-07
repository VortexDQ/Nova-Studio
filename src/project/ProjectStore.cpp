#include "nova/project/ProjectStore.h"

#include "nova/project/ProjectIO.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>

#include <algorithm>

namespace nova::project {

namespace {

constexpr const char* kRecentKey = "recentProjects";

QString settingsPath() {
    return QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
}

} // namespace

std::vector<std::string> ProjectStore::recentProjects(int maxCount) {
    QSettings settings(settingsPath(), QSettings::IniFormat);
    const QStringList entries = settings.value(kRecentKey).toStringList();

    std::vector<std::string> result;
    result.reserve(static_cast<size_t>(std::min<int>(maxCount, static_cast<int>(entries.size()))));
    for (const QString& entry : entries) {
        if (QFileInfo::exists(entry)) {
            result.push_back(entry.toStdString());
        }
        if (static_cast<int>(result.size()) >= maxCount) break;
    }
    return result;
}

void ProjectStore::addRecentProject(const std::string& path) {
    QSettings settings(settingsPath(), QSettings::IniFormat);
    QStringList entries = settings.value(kRecentKey).toStringList();
    const QString qPath = QString::fromStdString(path);
    entries.removeAll(qPath);
    entries.prepend(qPath);
    while (entries.size() > 20) {
        entries.removeLast();
    }
    settings.setValue(kRecentKey, entries);
}

void ProjectStore::removeRecentProject(const std::string& path) {
    QSettings settings(settingsPath(), QSettings::IniFormat);
    QStringList entries = settings.value(kRecentKey).toStringList();
    entries.removeAll(QString::fromStdString(path));
    settings.setValue(kRecentKey, entries);
}

std::string ProjectStore::autosavePathFor(const std::string& projectPath) {
    const QFileInfo info(QString::fromStdString(projectPath));
    return info.absolutePath().toStdString() + "/" + info.completeBaseName().toStdString()
           + ".autosave.nova";
}

std::string ProjectStore::backupDirectoryFor(const std::string& projectPath) {
    const QFileInfo info(QString::fromStdString(projectPath));
    return info.absolutePath().toStdString() + "/" + info.completeBaseName().toStdString()
           + "_backups";
}

bool ProjectStore::autosave(const Project& project, std::string* error) {
    if (project.filePath.empty()) {
        if (error) *error = "Project has no save path; use Save As first.";
        return false;
    }
    return ProjectIO::save(project, autosavePathFor(project.filePath), error);
}

bool ProjectStore::createBackup(const Project& project, std::string* error) {
    if (project.filePath.empty()) {
        if (error) *error = "Project has no save path.";
        return false;
    }

    const QString backupDir = QString::fromStdString(backupDirectoryFor(project.filePath));
    QDir().mkpath(backupDir);

    const QString timestamp = QDateTime::currentDateTimeUtc().toString("yyyyMMdd_HHmmss");
    const QString backupPath =
        backupDir + "/" + QFileInfo(QString::fromStdString(project.filePath)).completeBaseName()
        + "_" + timestamp + ".nova";

    if (!ProjectIO::save(project, backupPath.toStdString(), error)) {
        return false;
    }

    // Keep the last 20 backups.
    QDir dir(backupDir);
    QStringList backups =
        dir.entryList({"*.nova"}, QDir::Files, QDir::Time | QDir::Reversed);
    while (backups.size() > 20) {
        dir.remove(backups.takeFirst());
    }
    return true;
}

std::vector<ProjectStore::VersionEntry> ProjectStore::versionHistory(
    const std::string& projectPath) {
    std::vector<VersionEntry> entries;
    const QDir dir(QString::fromStdString(backupDirectoryFor(projectPath)));
    if (!dir.exists()) return entries;

    const QStringList backups = dir.entryList({"*.nova"}, QDir::Files, QDir::Time);
    for (const QString& name : backups) {
        VersionEntry entry;
        entry.path = dir.filePath(name).toStdString();
        entry.label = name.toStdString();
        entries.push_back(std::move(entry));
    }
    return entries;
}

std::optional<Project> ProjectStore::restoreVersion(const std::string& backupPath,
                                                      std::string* error) {
    return ProjectIO::load(backupPath, error);
}

} // namespace nova::project
