#include "nova/ui/SidebarPanel.h"

#include "nova/media/StockCatalog.h"

#include <QButtonGroup>
#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QFrame>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QStackedWidget>
#include <QToolButton>
#include <QVBoxLayout>

namespace nova::ui {

SidebarPanel::SidebarPanel(QWidget* parent) : QWidget(parent) {
    buildUi();
}

QListWidget* SidebarPanel::mediaList() const { return mediaList_; }
QLineEdit* SidebarPanel::mediaSearch() const { return mediaSearch_; }
QComboBox* SidebarPanel::mediaFolderFilter() const { return mediaFolderFilter_; }
QListWidget* SidebarPanel::templateList() const { return templateList_; }
QListWidget* SidebarPanel::textPresetList() const { return textPresetList_; }
QListWidget* SidebarPanel::transitionList() const { return transitionList_; }

QToolButton* SidebarPanel::addRailButton(const QString& label, int index,
                                         QVBoxLayout* railLayout) {
    auto* button = new QToolButton(this);
    button->setText(label);
    button->setToolButtonStyle(Qt::ToolButtonTextOnly);
    button->setCheckable(true);
    button->setAutoRaise(true);
    button->setFixedWidth(68);
    button->setMinimumHeight(48);
    button->setFont(QFont(QStringLiteral("Segoe UI"), 8, QFont::DemiBold));
    categoryGroup_->addButton(button, index);
    railLayout->addWidget(button);
    return button;
}

void SidebarPanel::buildUi() {
    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* rail = new QFrame(this);
    rail->setObjectName("sidebarRail");
    rail->setFixedWidth(72);
    auto* railLayout = new QVBoxLayout(rail);
    railLayout->setContentsMargins(2, 6, 2, 6);
    railLayout->setSpacing(2);

    categoryGroup_ = new QButtonGroup(this);
    stack_ = new QStackedWidget(this);

    addRailButton(tr("Media"), 0, railLayout);
    stack_->addWidget(makeMediaPage());
    stack_->addWidget(makeRecordPage());
    addRailButton(tr("Record"), 1, railLayout);
    stack_->addWidget(makeLibraryPage());
    addRailButton(tr("Stock"), 2, railLayout);
    stack_->addWidget(makeTemplatesPage());
    addRailButton(tr("Templ"), 3, railLayout);
    stack_->addWidget(makeTextPage());
    addRailButton(tr("Text"), 4, railLayout);
    stack_->addWidget(makeTransitionsPage());
    addRailButton(tr("Trans"), 5, railLayout);
    stack_->addWidget(makeAIPage());
    addRailButton(tr("AI"), 6, railLayout);

    railLayout->addStretch();

    connect(categoryGroup_, &QButtonGroup::idClicked, stack_, &QStackedWidget::setCurrentIndex);
    if (auto* first = categoryGroup_->button(0)) {
        first->setChecked(true);
    }

    root->addWidget(rail);
    root->addWidget(stack_, 1);
}

QWidget* SidebarPanel::makeMediaPage() {
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(10, 10, 10, 10);

    auto* header = new QLabel(tr("My media"), page);
    header->setStyleSheet("font-size: 13pt; font-weight: 600;");
    layout->addWidget(header);

    auto* importBtn = new QPushButton(tr("+ Import file"), page);
    connect(importBtn, &QPushButton::clicked, this, &SidebarPanel::importMediaRequested);
    layout->addWidget(importBtn);

    mediaSearch_ = new QLineEdit(page);
    mediaSearch_->setPlaceholderText(tr("Search by name or tag"));
    layout->addWidget(mediaSearch_);

    mediaFolderFilter_ = new QComboBox(page);
    mediaFolderFilter_->addItem(tr("All folders"), QString());
    mediaFolderFilter_->addItem(tr("Imports"), QStringLiteral("Imports"));
    mediaFolderFilter_->addItem(tr("Screen recordings"), QStringLiteral("Screen recordings"));
    mediaFolderFilter_->addItem(tr("Camera"), QStringLiteral("Camera"));
    mediaFolderFilter_->addItem(tr("Drone"), QStringLiteral("Drone"));
    mediaFolderFilter_->addItem(tr("Mobile"), QStringLiteral("Mobile"));
    mediaFolderFilter_->addItem(tr("Stock"), QStringLiteral("Stock"));
    layout->addWidget(mediaFolderFilter_);

    mediaList_ = new QListWidget(page);
    layout->addWidget(mediaList_, 1);

    layout->addWidget(new QLabel(tr("Click to preview. Double-click to add to timeline."), page));
    return page;
}

QWidget* SidebarPanel::makeRecordPage() {
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(10, 10, 10, 10);

    auto* header = new QLabel(tr("Record"), page);
    header->setStyleSheet("font-size: 13pt; font-weight: 600;");
    layout->addWidget(header);
    layout->addWidget(new QLabel(tr("Capture footage directly into your project:"), page));

    auto addRecord = [&](const QString& label, int mode) {
        auto* btn = new QPushButton(label, page);
        connect(btn, &QPushButton::clicked, this, [this, mode]() { emit recordRequested(mode); });
        layout->addWidget(btn);
    };

    addRecord(tr("Record screen"), 0);
    addRecord(tr("Record camera"), 1);
    addRecord(tr("Record screen + camera"), 2);
    addRecord(tr("Record voice / microphone"), 3);

    auto* stopBtn = new QPushButton(tr("Stop recording"), page);
    connect(stopBtn, &QPushButton::clicked, this, [this]() { emit stopRecordRequested(); });
    layout->addWidget(stopBtn);

    layout->addSpacing(12);
    layout->addWidget(new QLabel(tr("Or import existing files:"), page));

    auto addImport = [&](const QString& label, const QString& folder) {
        auto* btn = new QPushButton(label, page);
        connect(btn, &QPushButton::clicked, this, [this, folder]() {
            emit importMediaToFolder(folder);
        });
        layout->addWidget(btn);
    };
    addImport(tr("Import video file…"), QStringLiteral("Imports"));
    addImport(tr("Import audio…"), QStringLiteral("Imports"));
    addImport(tr("Import image…"), QStringLiteral("Imports"));

    layout->addStretch();
    return page;
}

QWidget* SidebarPanel::makeAIPage() {
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(10, 10, 10, 10);

    auto* header = new QLabel(tr("AI tools"), page);
    header->setStyleSheet("font-size: 13pt; font-weight: 600;");
    layout->addWidget(header);
    layout->addWidget(new QLabel(tr("Optional AI plugins (offline core stays fast):"), page));

    auto* list = new QListWidget(page);
    const struct { const char* id; const char* label; } tools[] = {
        {"tts", "AI text to speech"},
        {"auto-compose", "AI auto compose"},
        {"silence-remover", "AI silence remover"},
        {"noise-remover", "AI background noise remover"},
        {"subtitles", "AI subtitles"},
        {"bg-remove", "AI background removal"},
    };
    for (const auto& tool : tools) {
        auto* item = new QListWidgetItem(tr(tool.label), list);
        item->setData(Qt::UserRole, QString::fromLatin1(tool.id));
    }
    connect(list, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
        if (item) emit aiToolRequested(item->data(Qt::UserRole).toString());
    });
    layout->addWidget(list, 1);
    return page;
}

QWidget* SidebarPanel::makeLibraryPage() {
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(10, 10, 10, 10);

    auto* header = new QLabel(tr("Stock clips"), page);
    header->setStyleSheet("font-size: 13pt; font-weight: 600;");
    layout->addWidget(header);
    layout->addWidget(new QLabel(tr("Click to insert at the playhead:"), page));

    auto* list = new QListWidget(page);
    for (const auto& asset : nova::media::kStockAssets) {
        auto* item = new QListWidgetItem(tr(asset.label), list);
        item->setData(Qt::UserRole, QString::fromLatin1(asset.id));
    }
    connect(list, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
        if (item) emit libraryAssetActivated(item->data(Qt::UserRole).toString());
    });
    layout->addWidget(list, 1);
    return page;
}

QWidget* SidebarPanel::makeTemplatesPage() {
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(10, 10, 10, 10);

    auto* header = new QLabel(tr("Templates"), page);
    header->setStyleSheet("font-size: 13pt; font-weight: 600;");
    layout->addWidget(header);

    templateList_ = new QListWidget(page);
    const QDir bundledTemplateDir(QCoreApplication::applicationDirPath() + QStringLiteral("/templates"));
    const QDir templateDir(QCoreApplication::applicationDirPath() + QStringLiteral("/../templates"));
    const QDir sourceTemplateDir(QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("../../templates")));

    auto addTemplatesFrom = [&](const QDir& dir) {
        if (!dir.exists()) return;
        for (const QString& file : dir.entryList({"*.nova"}, QDir::Files)) {
            const QString path = dir.filePath(file);
            auto* item = new QListWidgetItem(QFileInfo(file).completeBaseName(), templateList_);
            item->setData(Qt::UserRole, path);
        }
    };
    addTemplatesFrom(bundledTemplateDir);
    addTemplatesFrom(templateDir);
    addTemplatesFrom(sourceTemplateDir);

    if (templateList_->count() == 0) {
        const struct { const char* id; const char* name; } builtins[] = {
            {"__builtin_blank_1080p__", "Blank 1080p"},
            {"__builtin_vertical_916__", "Vertical Reels 9:16"},
            {"__builtin_youtube_1080p__", "YouTube 1080p"},
        };
        for (const auto& b : builtins) {
            auto* item = new QListWidgetItem(tr(b.name), templateList_);
            item->setData(Qt::UserRole, QString::fromLatin1(b.id));
        }
    }

    connect(templateList_, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
        if (item) emit templateActivated(item->data(Qt::UserRole).toString());
    });

    auto* useBtn = new QPushButton(tr("Create project from selected"), page);
    connect(useBtn, &QPushButton::clicked, this, [this]() {
        if (auto* item = templateList_->currentItem()) {
            emit templateActivated(item->data(Qt::UserRole).toString());
        }
    });
    layout->addWidget(templateList_, 1);
    layout->addWidget(useBtn);
    return page;
}

void SidebarPanel::populateTextPresets() {
    const struct { const char* id; const char* label; const char* text; } presets[] = {
        {"lower-third-minimal", "Lower third - Minimal", "Your name here"},
        {"lower-third-broadcast", "Lower third - Broadcast", "Segment title"},
        {"quote", "Quote", "Your quote here"},
        {"rating", "Rating stars", "Product name"},
        {"credits", "Rolling credits", "Directed by You"},
        {"timer", "Timer", "01:00"},
        {"meme", "Meme top/bottom", "TOP TEXT / BOTTOM TEXT"},
        {"intro-clean", "Intro - Clean", "My Video"},
        {"outro-stamp", "Outro - Stamp", "Thanks for watching"},
    };
    for (const auto& p : presets) {
        auto* item = new QListWidgetItem(tr(p.label), textPresetList_);
        item->setData(Qt::UserRole, QString::fromLatin1(p.id));
        item->setData(Qt::UserRole + 1, QString::fromUtf8(p.text));
    }
}

QWidget* SidebarPanel::makeTextPage() {
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(10, 10, 10, 10);

    auto* header = new QLabel(tr("Text"), page);
    header->setStyleSheet("font-size: 13pt; font-weight: 600;");
    layout->addWidget(header);
    layout->addWidget(new QLabel(tr("Click a preset to add it at the playhead:"), page));

    textPresetList_ = new QListWidget(page);
    populateTextPresets();
    connect(textPresetList_, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
        if (!item) return;
        emit textPresetActivated(item->data(Qt::UserRole).toString(),
                                 item->data(Qt::UserRole + 1).toString());
    });
    layout->addWidget(textPresetList_, 1);
    return page;
}

void SidebarPanel::populateTransitions() {
    const struct { const char* id; const char* label; } transitions[] = {
        {"cross-dissolve", "Cross dissolve"},
        {"fade-in", "Fade in"},
        {"fade-out", "Fade out"},
        {"dip-black", "Dip to black"},
        {"dip-white", "Dip to white"},
        {"wipe", "Wipe"},
        {"slide", "Slide"},
        {"push", "Push"},
        {"zoom", "Zoom"},
        {"spin", "Spin"},
        {"blur", "Blur"},
        {"glitch", "Glitch"},
        {"light-leak", "Light leak"},
        {"film-burn", "Film burn"},
    };
    for (const auto& t : transitions) {
        auto* item = new QListWidgetItem(tr(t.label), transitionList_);
        item->setData(Qt::UserRole, QString::fromLatin1(t.id));
    }
}

QWidget* SidebarPanel::makeTransitionsPage() {
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(10, 10, 10, 10);

    auto* header = new QLabel(tr("Transitions"), page);
    header->setStyleSheet("font-size: 13pt; font-weight: 600;");
    layout->addWidget(header);
    layout->addWidget(new QLabel(tr("Select a clip, move playhead to a cut, then click:"), page));

    transitionList_ = new QListWidget(page);
    populateTransitions();
    connect(transitionList_, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
        if (item) emit transitionActivated(item->data(Qt::UserRole).toString());
    });
    layout->addWidget(transitionList_, 1);
    return page;
}

} // namespace nova::ui
