#include "nova/ui/SidebarPanel.h"

#include <QButtonGroup>
#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QStackedWidget>
#include <QToolButton>
#include <QVBoxLayout>

namespace nova::ui {

namespace {

QToolButton* makeCategoryButton(const QString& text, const QString& glyph, QWidget* parent) {
    auto* button = new QToolButton(parent);
    button->setText(text + "\n" + glyph);
    button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    button->setCheckable(true);
    button->setAutoRaise(true);
    button->setFixedWidth(72);
    button->setMinimumHeight(72);
    return button;
}

} // namespace

SidebarPanel::SidebarPanel(QWidget* parent) : QWidget(parent) {
    buildUi();
}

QWidget* SidebarPanel::mediaPage() const { return stack_->widget(0); }
QListWidget* SidebarPanel::mediaList() const { return mediaList_; }
QLineEdit* SidebarPanel::mediaSearch() const { return mediaSearch_; }
QComboBox* SidebarPanel::mediaFolderFilter() const { return mediaFolderFilter_; }
QListWidget* SidebarPanel::templateList() const { return templateList_; }
QListWidget* SidebarPanel::textPresetList() const { return textPresetList_; }
QListWidget* SidebarPanel::transitionList() const { return transitionList_; }

void SidebarPanel::buildUi() {
    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* rail = new QFrame(this);
    rail->setObjectName("sidebarRail");
    rail->setFixedWidth(80);
    auto* railLayout = new QVBoxLayout(rail);
    railLayout->setContentsMargins(4, 8, 4, 8);
    railLayout->setSpacing(4);

    categoryGroup_ = new QButtonGroup(this);
    stack_ = new QStackedWidget(this);

    const struct Category {
        const char* label;
        const char* icon;
        QWidget* (SidebarPanel::*factory)();
    } categories[] = {
        {"Media", "📁", &SidebarPanel::makeMediaPage},
        {"Record", "🎥", nullptr},
        {"Library", "🎬", nullptr},
        {"Templates", "▦", &SidebarPanel::makeTemplatesPage},
        {"Text", "T", &SidebarPanel::makeTextPage},
        {"Transitions", "↔", &SidebarPanel::makeTransitionsPage},
    };

    int index = 0;
    for (const auto& category : categories) {
        auto* button = makeCategoryButton(tr(category.label), QString::fromUtf8(category.icon), rail);
        categoryGroup_->addButton(button, index);
        railLayout->addWidget(button);

        if (category.factory) {
            stack_->addWidget((this->*category.factory)());
        } else {
            stack_->addWidget(makePlaceholderPage(
                tr(category.label),
                tr("Coming in a future milestone. Offline projects work locally today.")));
        }
        ++index;
    }
    railLayout->addStretch();

    connect(categoryGroup_, &QButtonGroup::idClicked, stack_, &QStackedWidget::setCurrentIndex);
    connect(categoryGroup_, &QButtonGroup::idClicked, this, &SidebarPanel::categoryChanged);

    if (auto* first = categoryGroup_->button(0)) {
        first->setChecked(true);
    }

    root->addWidget(rail);
    root->addWidget(stack_, 1);
}

QWidget* SidebarPanel::makePlaceholderPage(const QString& title, const QString& description) {
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    layout->addWidget(new QLabel("<b>" + title + "</b>", page));
    layout->addWidget(new QLabel(description, page));
    layout->addStretch();
    return page;
}

QWidget* SidebarPanel::makeMediaPage() {
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    layout->addWidget(new QLabel(tr("<b>My Media</b>"), page));

    mediaSearch_ = new QLineEdit(page);
    mediaSearch_->setPlaceholderText(tr("Search media, tags, folders..."));
    layout->addWidget(mediaSearch_);

    mediaFolderFilter_ = new QComboBox(page);
    mediaFolderFilter_->addItem(tr("All folders"), QString());
    mediaFolderFilter_->addItem(tr("Imports"), QStringLiteral("Imports"));
    mediaFolderFilter_->addItem(tr("Screen recordings"), QStringLiteral("Screen recordings"));
    mediaFolderFilter_->addItem(tr("Camera"), QStringLiteral("Camera"));
    mediaFolderFilter_->addItem(tr("Drone"), QStringLiteral("Drone"));
    mediaFolderFilter_->addItem(tr("Mobile"), QStringLiteral("Mobile"));
    layout->addWidget(mediaFolderFilter_);

    mediaList_ = new QListWidget(page);
    layout->addWidget(mediaList_, 1);
    return page;
}

QWidget* SidebarPanel::makeTemplatesPage() {
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    layout->addWidget(new QLabel(tr("<b>Project Templates</b>"), page));
    layout->addWidget(new QLabel(tr("Start from a preset sequence size."), page));

    templateList_ = new QListWidget(page);
    const QDir templateDir(QCoreApplication::applicationDirPath() + "/../templates");
    const QDir sourceTemplateDir(QDir(QCoreApplication::applicationDirPath()).filePath("../../templates"));

    auto addTemplatesFrom = [&](const QDir& dir) {
        if (!dir.exists()) return;
        for (const QString& file : dir.entryList({"*.nova"}, QDir::Files)) {
            const QString path = dir.filePath(file);
            auto* item = new QListWidgetItem(QFileInfo(file).completeBaseName(), templateList_);
            item->setData(Qt::UserRole, path);
        }
    };
    addTemplatesFrom(templateDir);
    addTemplatesFrom(sourceTemplateDir);

    if (templateList_->count() == 0) {
        templateList_->addItem(tr("Blank 1080p (built-in)"));
        templateList_->item(0)->setData(Qt::UserRole, QString("__builtin_blank_1080p__"));
        templateList_->addItem(tr("Vertical Reels 9:16"));
        templateList_->item(1)->setData(Qt::UserRole, QString("__builtin_vertical_916__"));
        templateList_->addItem(tr("YouTube 1080p"));
        templateList_->item(2)->setData(Qt::UserRole, QString("__builtin_youtube_1080p__"));
    }

    connect(templateList_, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* item) {
        if (!item) return;
        emit templateActivated(item->data(Qt::UserRole).toString());
    });

    layout->addWidget(templateList_, 1);
    return page;
}

QWidget* SidebarPanel::makeTextPage() {
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    layout->addWidget(new QLabel(tr("<b>Text</b>"), page));

    textPresetList_ = new QListWidget(page);
    const QStringList presets = {
        tr("Lower third — Minimal"),
        tr("Lower third — Broadcast"),
        tr("Quote"),
        tr("Rating stars"),
        tr("Rolling credits"),
        tr("Timer"),
        tr("Meme top/bottom"),
        tr("Intro — Clean"),
        tr("Outro — Stamp"),
    };
    for (const QString& preset : presets) {
        auto* item = new QListWidgetItem(preset, textPresetList_);
        item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
    }
    layout->addWidget(new QLabel(tr("Presets preview (editable titles coming soon)"), page));
    layout->addWidget(textPresetList_, 1);
    return page;
}

QWidget* SidebarPanel::makeTransitionsPage() {
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    layout->addWidget(new QLabel(tr("<b>Transitions</b>"), page));

    transitionList_ = new QListWidget(page);
    const QStringList transitions = {
        tr("Cross dissolve"), tr("Fade in"), tr("Fade out"), tr("Dip to black"),
        tr("Dip to white"), tr("Wipe"), tr("Slide"), tr("Push"), tr("Zoom"),
        tr("Spin"), tr("Blur"), tr("Glitch"), tr("Light leak"), tr("Film burn"),
    };
    for (const QString& name : transitions) {
        auto* item = new QListWidgetItem(name, transitionList_);
        item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
    }
    layout->addWidget(new QLabel(tr("Drag onto cut points (timeline engine milestone)"), page));
    layout->addWidget(transitionList_, 1);
    return page;
}

} // namespace nova::ui
