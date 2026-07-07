#pragma once

#include <QWidget>

class QStackedWidget;
class QButtonGroup;
class QToolButton;
class QListWidget;
class QLineEdit;
class QComboBox;
class QPushButton;
class QVBoxLayout;

namespace nova::ui {

class SidebarPanel : public QWidget {
    Q_OBJECT

public:
    explicit SidebarPanel(QWidget* parent = nullptr);

    QListWidget* mediaList() const;
    QLineEdit* mediaSearch() const;
    QComboBox* mediaFolderFilter() const;
    QListWidget* templateList() const;
    QListWidget* textPresetList() const;
    QListWidget* transitionList() const;

signals:
    void importMediaRequested();
    void importMediaToFolder(const QString& folder);
    void templateActivated(const QString& templatePath);
    void textPresetActivated(const QString& presetId, const QString& defaultText);
    void transitionActivated(const QString& transitionId);
    void libraryAssetActivated(const QString& assetId);

private:
    void buildUi();
    QToolButton* addRailButton(const QString& label, int index, QVBoxLayout* railLayout);
    QWidget* makeMediaPage();
    QWidget* makeRecordPage();
    QWidget* makeLibraryPage();
    QWidget* makeTemplatesPage();
    QWidget* makeTextPage();
    QWidget* makeTransitionsPage();
    void populateTextPresets();
    void populateTransitions();

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
