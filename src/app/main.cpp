#include <QApplication>
#include <QSurfaceFormat>

#include "nova/ui/MainWindow.h"
#include "nova/ui/ProjectWelcomeDialog.h"
#include "nova/core/Logger.h"

#include <QDialog>

int main(int argc, char** argv) {
    QSurfaceFormat format;
    format.setVersion(3, 3);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setSwapInterval(1);
    QSurfaceFormat::setDefaultFormat(format);

    QApplication app(argc, argv);
    QApplication::setApplicationName("Nova Studio");
    QApplication::setOrganizationName("Nova Studio Project");

    NOVA_LOG_INFO("app.main", "Starting Nova Studio");

    nova::ui::MainWindow window;

    bool opened = false;
    if (app.arguments().size() > 1) {
        opened = window.openProjectPath(app.arguments().at(1));
    } else {
        nova::ui::ProjectWelcomeDialog welcome;
        if (welcome.exec() == QDialog::Accepted) {
            const auto choice = welcome.choice();
            switch (choice.action) {
            case nova::ui::WelcomeChoice::Action::NewProject:
                opened = window.newProjectAtPath(QString::fromStdString(choice.path),
                                                   choice.frameRate, choice.width, choice.height);
                break;
            case nova::ui::WelcomeChoice::Action::OpenProject:
            case nova::ui::WelcomeChoice::Action::OpenRecent:
                opened = window.openProjectPath(QString::fromStdString(choice.path));
                break;
            case nova::ui::WelcomeChoice::Action::OpenTemplate:
                opened = window.newProjectFromTemplate(QString::fromStdString(choice.templateId));
                break;
            default:
                opened = true;
                break;
            }
        } else {
            opened = true;
        }
    }

    if (!opened) {
        return 0;
    }

    window.show();
    return QApplication::exec();
}
