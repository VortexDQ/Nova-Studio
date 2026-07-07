#include "nova/media/GifExporter.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QProcess>

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

bool GifExporter::exportGif(const std::string& inputPath, const std::string& outputPath,
                            double startSeconds, double endSeconds, int fps, int maxWidth,
                            std::string* error) {
    lastError_.clear();
    if (endSeconds <= startSeconds) {
        lastError_ = "Invalid GIF export range.";
        if (error) *error = lastError_;
        return false;
    }

    const QString ffmpeg = findFfmpegExecutable();
    if (ffmpeg.isEmpty()) {
        lastError_ = "ffmpeg is required for GIF export. Install ffmpeg or add it to PATH.";
        if (error) *error = lastError_;
        return false;
    }

    const QString filter = QStringLiteral("fps=%1,scale=%2:-1:flags=lanczos").arg(fps).arg(maxWidth);
    QStringList args;
    args << QStringLiteral("-y")
         << QStringLiteral("-ss") << QString::number(startSeconds, 'f', 3)
         << QStringLiteral("-t") << QString::number(endSeconds - startSeconds, 'f', 3)
         << QStringLiteral("-i") << QString::fromStdString(inputPath)
         << QStringLiteral("-vf") << filter
         << QStringLiteral("-loop") << QStringLiteral("0")
         << QString::fromStdString(outputPath);

    QProcess process;
    process.start(ffmpeg, args);
    if (!process.waitForFinished(180000) || process.exitCode() != 0) {
        lastError_ = QString::fromLocal8Bit(process.readAllStandardError()).toStdString();
        if (lastError_.empty()) lastError_ = "ffmpeg GIF export failed.";
        if (error) *error = lastError_;
        return false;
    }
    return true;
}

} // namespace nova::media
