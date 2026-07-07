#pragma once

#include <QWidget>

class QStackedWidget;
class QButtonGroup;
class QToolButton;
class QListWidget;
class QLineEdit;
class QComboBox;
class QLabel;

namespace nova::ui {

// Clipchamp-style left rail: icon categories with a stacked content panel.
class SidebarPanel : public QWidget {
    Q_OBJECT

public:
    explicit SidebarPanel(QWidget* parent = nullptr);

    QWidget* mediaPage() const;
    QListWidget* mediaList() const;
    QLineEdit* mediaSearch() const;
    QComboBox* mediaFolderFilter() const;
    QListWidget* templateList() const;
    QListWidget* textPresetList() const;
    QListWidget* transitionList() const;

signals:
    void categoryChanged(int index);
    void templateActivated(const QString& templatePath);

private:
    void buildUi();
    QWidget* makePlaceholderPage(const QString& title, const QString& description);
    QWidget* makeMediaPage();
    QWidget* makeTemplatesPage();
    QWidget* makeTextPage();
    QWidget* makeTransitionsPage();

    QStackedWidget* stack_ = nullptr;
    QButtonGroup* categoryGroup_ = nullptr;
    QListWidget* mediaList_ = nullptr;
    QLineEdit* mediaSearch_ = nullptr;
    QComboBox* mediaFolderFilter_ = nullptr;
    QListWidget* templateList_ = nullptr;
    QListWidget* textPresetList_ = nullptr;
    QListWidget* transitionList_ = nullptr;
};

} // namespace nova::ui
