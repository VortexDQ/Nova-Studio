#include "nova/media/AudioExporter.h"
#include "nova/media/AudioExtractor.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>

namespace nova::media {

namespace {

QString findFfmpegExecutable() {
    const QStringList candidates = {
        QCoreApplication::applicationDirPath() + QStringLiteral("/ffmpeg.exe"),
        QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("../../vcpkg_installed/x64-windows-release/tools/ffmpeg/ffmpeg.exe")),
        QStringLiteral("ffmpeg"),
    };
    for (const QString& candidate : candidates) {
        if (candidate == QStringLiteral("ffmpeg")) {
            QProcess which;
            which.start(QStringLiteral("where"), {QStringLiteral("ffmpeg")});
            if (which.waitForFinished(3000) && which.exitCode() == 0) {
                const QString path = QString::fromLocal8Bit(which.readAllStandardOutput()).trimmed().split('\n').value(0).trimmed();
                if (!path.isEmpty() && QFileInfo::exists(path)) return path;
            }
            continue;
        }
        if (QFileInfo::exists(candidate)) return candidate;
    }
    return {};
}

} // namespace

bool AudioExporter::exportMp3(const std::string& inputPath, const std::string& outputPath,
                              double startSeconds, double endSeconds, std::string* error) {
    lastError_.clear();
    const QString ffmpeg = findFfmpegExecutable();
    if (ffmpeg.isEmpty()) {
        AudioExtractor fallback;
        const std::string wavPath = outputPath + ".wav";
        if (!fallback.extractWav(inputPath, wavPath)) {
            lastError_ = "MP3 encoder unavailable. WAV fallback failed: " + fallback.lastError();
            if (error) *error = lastError_;
            return false;
        }
        lastError_ = "MP3 encoder unavailable. Exported WAV instead: " + wavPath;
        if (error) *error = lastError_;
        return false;
    }

    QStringList args;
    args << QStringLiteral("-y");
    if (startSeconds > 0.0) args << QStringLiteral("-ss") << QString::number(startSeconds, 'f', 3);
    args << QStringLiteral("-i") << QString::fromStdString(inputPath);
    if (endSeconds > startSeconds) {
        args << QStringLiteral("-t") << QString::number(endSeconds - startSeconds, 'f', 3);
    }
    args << QStringLiteral("-vn")
         << QStringLiteral("-acodec") << QStringLiteral("libmp3lame")
         << QStringLiteral("-q:a") << QStringLiteral("2")
         << QString::fromStdString(outputPath);

    QProcess process;
    process.start(ffmpeg, args);
    if (!process.waitForFinished(120000) || process.exitCode() != 0) {
        lastError_ = QString::fromLocal8Bit(process.readAllStandardError()).toStdString();
        if (lastError_.empty()) lastError_ = "ffmpeg MP3 export failed.";
        if (error) *error = lastError_;
        return false;
    }
    return true;
}

} // namespace nova::media
