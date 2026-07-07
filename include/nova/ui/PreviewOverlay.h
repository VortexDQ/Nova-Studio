#pragma once

#include <QWidget>

namespace nova::ui {

// Transparent overlay on the preview that draws active title/text presets.
class PreviewOverlay : public QWidget {
    Q_OBJECT

public:
    explicit PreviewOverlay(QWidget* parent = nullptr);

    void setTitle(const QString& text, const QString& stylePreset);
    void clearTitle();

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QString text_;
    QString stylePreset_;
    bool visible_ = false;
};

} // namespace nova::ui
