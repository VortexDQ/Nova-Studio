#include "nova/ui/ProjectWelcomeDialog.h"

#include "nova/project/ProjectStore.h"

#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

namespace nova::ui {

ProjectWelcomeDialog::ProjectWelcomeDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle(tr("Nova Studio"));
    resize(560, 420);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(tr("<h2>Welcome to Nova Studio</h2>"), this));
    layout->addWidget(new QLabel(tr("Create a local offline project or open a recent one."), this));

    recentList_ = new QListWidget(this);
    refreshRecentList();
    layout->addWidget(recentList_, 1);

    auto* buttons = new QHBoxLayout();
    auto* newButton = new QPushButton(tr("New Project..."), this);
    auto* openButton = new QPushButton(tr("Open Project..."), this);
    auto* templateButton = new QPushButton(tr("From Template..."), this);
    auto* cancelButton = new QPushButton(tr("Continue without project"), this);
    buttons->addWidget(newButton);
    buttons->addWidget(openButton);
    buttons->addWidget(templateButton);
    buttons->addStretch();
    buttons->addWidget(cancelButton);
    layout->addLayout(buttons);

    connect(newButton, &QPushButton::clicked, this, &ProjectWelcomeDialog::onNewProject);
    connect(openButton, &QPushButton::clicked, this, &ProjectWelcomeDialog::onOpenProject);
    connect(templateButton, &QPushButton::clicked, this, &ProjectWelcomeDialog::onOpenTemplate);
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    connect(recentList_, &QListWidget::itemDoubleClicked, this, &ProjectWelcomeDialog::onOpenRecent);
}

void ProjectWelcomeDialog::refreshRecentList() {
    recentList_->clear();
    for (const std::string& path : nova::project::ProjectStore::recentProjects()) {
        auto* item = new QListWidgetItem(QString::fromStdString(path), recentList_);
        item->setData(Qt::UserRole, QString::fromStdString(path));
        item->setToolTip(QString::fromStdString(path));
    }
    if (recentList_->count() == 0) {
        recentList_->addItem(tr("No recent projects yet"));
        recentList_->item(0)->setFlags(Qt::NoItemFlags);
    }
}

void ProjectWelcomeDialog::onNewProject() {
    const QString path = QFileDialog::getSaveFileName(
        this, tr("New Project"), QString(), tr("Nova Project (*.nova)"));
    if (path.isEmpty()) return;

    choice_.action = WelcomeChoice::Action::NewProject;
    choice_.path = path.toStdString();
    choice_.frameRate = 30.0;
    choice_.width = 1920;
    choice_.height = 1080;
    accept();
}

void ProjectWelcomeDialog::onOpenProject() {
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Open Project"), QString(), tr("Nova Project (*.nova)"));
    if (path.isEmpty()) return;

    choice_.action = WelcomeChoice::Action::OpenProject;
    choice_.path = path.toStdString();
    accept();
}

void ProjectWelcomeDialog::onOpenRecent() {
    auto* item = recentList_->currentItem();
    if (!item || !(item->flags() & Qt::ItemIsSelectable)) return;

    choice_.action = WelcomeChoice::Action::OpenRecent;
    choice_.path = item->data(Qt::UserRole).toString().toStdString();
    accept();
}

void ProjectWelcomeDialog::onOpenTemplate() {
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Open Template"), QString(), tr("Nova Template (*.nova)"));
    if (path.isEmpty()) return;

    choice_.action = WelcomeChoice::Action::OpenTemplate;
    choice_.templateId = path.toStdString();
    accept();
}

} // namespace nova::ui
