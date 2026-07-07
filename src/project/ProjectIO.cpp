#include "nova/project/ProjectIO.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <chrono>
#include <iomanip>
#include <sstream>

namespace nova::project {

namespace {

std::string clipTypeToString(nova::timeline::ClipType type) {
    switch (type) {
    case nova::timeline::ClipType::Video: return "video";
    case nova::timeline::ClipType::Audio: return "audio";
    case nova::timeline::ClipType::Image: return "image";
    case nova::timeline::ClipType::AdjustmentLayer: return "adjustment";
    case nova::timeline::ClipType::Title: return "title";
    case nova::timeline::ClipType::Nested: return "nested";
    }
    return "video";
}

std::optional<nova::timeline::ClipType> clipTypeFromString(const QString& value) {
    if (value == "video") return nova::timeline::ClipType::Video;
    if (value == "audio") return nova::timeline::ClipType::Audio;
    if (value == "image") return nova::timeline::ClipType::Image;
    if (value == "adjustment") return nova::timeline::ClipType::AdjustmentLayer;
    if (value == "title") return nova::timeline::ClipType::Title;
    if (value == "nested") return nova::timeline::ClipType::Nested;
    return std::nullopt;
}

std::string trackTypeToString(nova::timeline::TrackType type) {
    switch (type) {
    case nova::timeline::TrackType::Video: return "video";
    case nova::timeline::TrackType::Audio: return "audio";
    case nova::timeline::TrackType::Subtitle: return "subtitle";
    }
    return "video";
}

std::optional<nova::timeline::TrackType> trackTypeFromString(const QString& value) {
    if (value == "video") return nova::timeline::TrackType::Video;
    if (value == "audio") return nova::timeline::TrackType::Audio;
    if (value == "subtitle") return nova::timeline::TrackType::Subtitle;
    return std::nullopt;
}

QJsonObject clipToJson(const nova::timeline::Clip& clip) {
    QJsonObject obj;
    obj["id"] = QString::fromStdString(clip.id);
    obj["name"] = QString::fromStdString(clip.name);
    obj["type"] = QString::fromStdString(clipTypeToString(clip.type));
    obj["mediaPath"] = QString::fromStdString(clip.mediaPath);
    obj["sourceIn"] = static_cast<qint64>(clip.sourceIn);
    obj["sourceOut"] = static_cast<qint64>(clip.sourceOut);
    obj["timelineStart"] = static_cast<qint64>(clip.timelineStart);
    obj["timelineEnd"] = static_cast<qint64>(clip.timelineEnd);
    obj["speed"] = clip.speed;
    obj["locked"] = clip.locked;
    obj["muted"] = clip.muted;
    obj["colorLabel"] = clip.colorLabel;
    if (clip.linkedClipId) {
        obj["linkedClipId"] = QString::fromStdString(*clip.linkedClipId);
    }
    if (!clip.overlayText.empty()) {
        obj["overlayText"] = QString::fromStdString(clip.overlayText);
    }
    if (!clip.stylePreset.empty()) {
        obj["stylePreset"] = QString::fromStdString(clip.stylePreset);
    }
    if (!clip.transitionIn.empty()) {
        obj["transitionIn"] = QString::fromStdString(clip.transitionIn);
    }
    if (!clip.transitionOut.empty()) {
        obj["transitionOut"] = QString::fromStdString(clip.transitionOut);
    }
    if (clip.transitionDurationSec != 0.5) {
        obj["transitionDurationSec"] = clip.transitionDurationSec;
    }
    if (clip.rotationDegrees != 0.0) obj["rotationDegrees"] = clip.rotationDegrees;
    if (clip.chromaKeyEnabled) obj["chromaKeyEnabled"] = clip.chromaKeyEnabled;
    if (clip.chromaKeyStrength != 0.6) obj["chromaKeyStrength"] = clip.chromaKeyStrength;
    return obj;
}

bool clipFromJson(const QJsonObject& obj, nova::timeline::Clip& clip) {
    if (!obj.contains("id") || !obj.contains("timelineStart") || !obj.contains("timelineEnd")) {
        return false;
    }
    clip.id = obj["id"].toString().toStdString();
    clip.name = obj["name"].toString().toStdString();
    if (const auto type = clipTypeFromString(obj["type"].toString())) {
        clip.type = *type;
    }
    clip.mediaPath = obj["mediaPath"].toString().toStdString();
    clip.sourceIn = obj["sourceIn"].toInteger();
    clip.sourceOut = obj["sourceOut"].toInteger();
    clip.timelineStart = obj["timelineStart"].toInteger();
    clip.timelineEnd = obj["timelineEnd"].toInteger();
    clip.speed = obj["speed"].toDouble(1.0);
    clip.locked = obj["locked"].toBool(false);
    clip.muted = obj["muted"].toBool(false);
    clip.colorLabel = obj["colorLabel"].toInt(0);
    if (obj.contains("linkedClipId") && !obj["linkedClipId"].isNull()) {
        clip.linkedClipId = obj["linkedClipId"].toString().toStdString();
    } else {
        clip.linkedClipId.reset();
    }
    clip.overlayText = obj["overlayText"].toString().toStdString();
    clip.stylePreset = obj["stylePreset"].toString().toStdString();
    clip.transitionIn = obj["transitionIn"].toString().toStdString();
    clip.transitionOut = obj["transitionOut"].toString().toStdString();
    clip.transitionDurationSec = obj["transitionDurationSec"].toDouble(0.5);
    clip.rotationDegrees = obj["rotationDegrees"].toDouble(0.0);
    clip.chromaKeyEnabled = obj["chromaKeyEnabled"].toBool(false);
    clip.chromaKeyStrength = obj["chromaKeyStrength"].toDouble(0.6);
    return true;
}

QJsonObject trackToJson(const nova::timeline::Track& track) {
    QJsonObject obj;
    obj["id"] = QString::fromStdString(track.id());
    obj["name"] = QString::fromStdString(track.name());
    obj["type"] = QString::fromStdString(trackTypeToString(track.type()));
    obj["locked"] = track.locked();
    obj["muted"] = track.muted();
    obj["solo"] = track.solo();

    QJsonArray clips;
    for (const auto& clip : track.clips()) {
        clips.append(clipToJson(clip));
    }
    obj["clips"] = clips;
    return obj;
}

QJsonObject timelineToJson(const nova::timeline::Timeline& timeline) {
    QJsonObject obj;
    obj["id"] = QString::fromStdString(timeline.id());
    obj["name"] = QString::fromStdString(timeline.name());
    obj["frameRate"] = timeline.frameRate();
    obj["width"] = timeline.width();
    obj["height"] = timeline.height();

    QJsonArray tracks;
    for (const auto& track : timeline.tracks()) {
        tracks.append(trackToJson(*track));
    }
    obj["tracks"] = tracks;
    return obj;
}

std::unique_ptr<nova::timeline::Timeline> timelineFromJson(const QJsonObject& obj) {
    if (!obj.contains("id") || !obj.contains("frameRate")) {
        return nullptr;
    }

    auto timeline = std::make_unique<nova::timeline::Timeline>(
        obj["id"].toString().toStdString(),
        obj["name"].toString("Sequence").toStdString(),
        obj["frameRate"].toDouble(30.0),
        obj["width"].toInt(1920),
        obj["height"].toInt(1080));

    const QJsonArray tracks = obj["tracks"].toArray();
    for (const QJsonValue& trackValue : tracks) {
        const QJsonObject trackObj = trackValue.toObject();
        const auto trackType = trackTypeFromString(trackObj["type"].toString());
        if (!trackType) continue;

        auto& track = timeline->addTrack(
            trackObj["id"].toString().toStdString(),
            trackObj["name"].toString().toStdString(),
            *trackType);
        track.setLocked(trackObj["locked"].toBool(false));
        track.setMuted(trackObj["muted"].toBool(false));
        track.setSolo(trackObj["solo"].toBool(false));

        const QJsonArray clips = trackObj["clips"].toArray();
        for (const QJsonValue& clipValue : clips) {
            nova::timeline::Clip clip;
            if (clipFromJson(clipValue.toObject(), clip)) {
                track.addClip(std::move(clip), true);
            }
        }
    }
    return timeline;
}

QJsonObject mediaToJson(const MediaAsset& asset) {
    QJsonObject obj;
    obj["id"] = QString::fromStdString(asset.id);
    obj["path"] = QString::fromStdString(asset.path);
    obj["name"] = QString::fromStdString(asset.name);
    obj["folder"] = QString::fromStdString(asset.folder);
    obj["width"] = asset.width;
    obj["height"] = asset.height;
    obj["frameRate"] = asset.frameRate;
    obj["durationSeconds"] = asset.durationSeconds;
    obj["hasVideo"] = asset.hasVideo;
    obj["hasAudio"] = asset.hasAudio;

    QJsonArray tags;
    for (const auto& tag : asset.tags) {
        tags.append(QString::fromStdString(tag));
    }
    obj["tags"] = tags;
    return obj;
}

MediaAsset mediaFromJson(const QJsonObject& obj) {
    MediaAsset asset;
    asset.id = obj["id"].toString().toStdString();
    asset.path = obj["path"].toString().toStdString();
    asset.name = obj["name"].toString().toStdString();
    asset.folder = obj["folder"].toString("Imports").toStdString();
    asset.width = obj["width"].toInt();
    asset.height = obj["height"].toInt();
    asset.frameRate = obj["frameRate"].toDouble();
    asset.durationSeconds = obj["durationSeconds"].toDouble();
    asset.hasVideo = obj["hasVideo"].toBool();
    asset.hasAudio = obj["hasAudio"].toBool();

    const QJsonArray tags = obj["tags"].toArray();
    for (const QJsonValue& tag : tags) {
        asset.tags.push_back(tag.toString().toStdString());
    }
    return asset;
}

QJsonObject projectToJson(const Project& project) {
    QJsonObject root;
    root["formatVersion"] = project.formatVersion;
    root["name"] = QString::fromStdString(project.name);

    QJsonObject metadata;
    metadata["author"] = QString::fromStdString(project.metadata.author);
    metadata["description"] = QString::fromStdString(project.metadata.description);
    metadata["created"] = QString::fromStdString(project.metadata.createdIso);
    metadata["modified"] = QString::fromStdString(project.metadata.modifiedIso);
    root["metadata"] = metadata;

    QJsonArray media;
    for (const auto& asset : project.media) {
        media.append(mediaToJson(asset));
    }
    root["media"] = media;

    QJsonArray timelines;
    for (const auto& timeline : project.timelines) {
        timelines.append(timelineToJson(*timeline));
    }
    root["timelines"] = timelines;
    root["activeTimelineId"] = QString::fromStdString(project.activeTimelineId);
    root["lastPlayheadSeconds"] = project.lastPlayheadSeconds;
    root["lastPreviewMediaPath"] = QString::fromStdString(project.lastPreviewMediaPath);
    root["selectedClipId"] = QString::fromStdString(project.selectedClipId);

    QJsonObject brand;
    brand["logoPath"] = QString::fromStdString(project.brand.logoPath);
    brand["primaryColor"] = QString::fromStdString(project.brand.primaryColor);
    brand["fontFamily"] = QString::fromStdString(project.brand.fontFamily);
    root["brand"] = brand;
    return root;
}

std::unique_ptr<Project> projectFromJson(const QJsonObject& root) {
    auto project = std::make_unique<Project>();
    project->formatVersion = root["formatVersion"].toInt(1);
    project->name = root["name"].toString("Untitled").toStdString();

    const QJsonObject metadata = root["metadata"].toObject();
    project->metadata.author = metadata["author"].toString().toStdString();
    project->metadata.description = metadata["description"].toString().toStdString();
    project->metadata.createdIso = metadata["created"].toString().toStdString();
    project->metadata.modifiedIso = metadata["modified"].toString().toStdString();

    const QJsonArray media = root["media"].toArray();
    for (const QJsonValue& value : media) {
        project->media.push_back(mediaFromJson(value.toObject()));
    }

    const QJsonArray timelines = root["timelines"].toArray();
    for (const QJsonValue& value : timelines) {
        if (auto timeline = timelineFromJson(value.toObject())) {
            project->timelines.push_back(std::move(timeline));
        }
    }

    project->activeTimelineId = root["activeTimelineId"].toString().toStdString();
    project->lastPlayheadSeconds = root["lastPlayheadSeconds"].toDouble(0.0);
    project->lastPreviewMediaPath = root["lastPreviewMediaPath"].toString().toStdString();
    project->selectedClipId = root["selectedClipId"].toString().toStdString();
    if (const QJsonObject brand = root["brand"].toObject(); !brand.isEmpty()) {
        project->brand.logoPath = brand["logoPath"].toString().toStdString();
        project->brand.primaryColor = brand["primaryColor"].toString("#4a69d4").toStdString();
        project->brand.fontFamily = brand["fontFamily"].toString("Segoe UI").toStdString();
    }
    project->dirty = false;
    return project;
}

std::string currentIsoTimestamp() {
    const auto now = std::chrono::system_clock::now();
    const auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#if defined(_WIN32)
    gmtime_s(&tm, &time);
#else
    gmtime_r(&time, &tm);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

} // namespace

bool ProjectIO::save(const Project& project, const std::string& path, std::string* error) {
    QJsonObject root = projectToJson(project);
    QJsonObject metadata = root["metadata"].toObject();
    metadata["modified"] = QString::fromStdString(currentIsoTimestamp());
    root["metadata"] = metadata;

    QFile file(QString::fromStdString(path));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error) *error = "Could not open file for writing: " + path;
        return false;
    }

    const QJsonDocument doc(root);
    if (file.write(doc.toJson(QJsonDocument::Indented)) < 0) {
        if (error) *error = "Failed to write project file: " + path;
        return false;
    }
    return true;
}

std::optional<Project> ProjectIO::load(const std::string& path, std::string* error) {
    QFile file(QString::fromStdString(path));
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) *error = "Could not open project: " + path;
        return std::nullopt;
    }

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        if (error) *error = "Invalid project JSON: " + parseError.errorString().toStdString();
        return std::nullopt;
    }

    auto project = projectFromJson(doc.object());
    if (!project) {
        if (error) *error = "Project file is empty or corrupt.";
        return std::nullopt;
    }

    project->filePath = path;
    project->dirty = false;
    return std::move(*project);
}

std::optional<Project> ProjectIO::loadTemplate(const std::string& templatePath,
                                                 std::string* error) {
    auto project = load(templatePath, error);
    if (!project) return std::nullopt;
    project->filePath.clear();
    project->dirty = true;
    return project;
}

} // namespace nova::project
