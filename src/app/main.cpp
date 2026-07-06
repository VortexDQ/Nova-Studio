#include <QApplication>
#include <QSurfaceFormat>

#include "nova/ui/MainWindow.h"
#include "nova/core/Logger.h"

int main(int argc, char** argv) {
    // Request a modern core-profile OpenGL context before any QOpenGLWidget
    // is constructed, as required by Qt.
    QSurfaceFormat format;
    format.setVersion(3, 3);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setSwapInterval(1); // vsync
    QSurfaceFormat::setDefaultFormat(format);

    QApplication app(argc, argv);
    QApplication::setApplicationName("Nova Studio");
    QApplication::setOrganizationName("Nova Studio Project");

    NOVA_LOG_INFO("app.main", "Starting Nova Studio");

    nova::ui::MainWindow window;
    window.show();

    return QApplication::exec();
}
