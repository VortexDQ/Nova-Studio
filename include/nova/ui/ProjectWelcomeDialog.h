#pragma once

#include <optional>
#include <string>

#include <QDialog>

class QListWidget;
class QPushButton;

namespace nova::ui {

struct WelcomeChoice {
    enum class Action { Cancel, NewProject, OpenProject, OpenRecent, OpenTemplate };
    Action action = Action::Cancel;
    std::string path;
    std::string templateId;
    double frameRate = 30.0;
    int width = 1920;
    int height = 1080;
};

class ProjectWelcomeDialog : public QDialog {
    Q_OBJECT

public:
    explicit ProjectWelcomeDialog(QWidget* parent = nullptr);

    WelcomeChoice choice() const { return choice_; }

private slots:
    void onNewProject();
    void onOpenProject();
    void onOpenRecent();
    void onOpenTemplate();

private:
    void refreshRecentList();

    QListWidget* recentList_ = nullptr;
    WelcomeChoice choice_;
};

} // namespace nova::ui
