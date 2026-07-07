#include "nova/ui/PreviewOverlay.h"

#include <QPainter>
#include <QPainterPath>

namespace nova::ui {

PreviewOverlay::PreviewOverlay(QWidget* parent) : QWidget(parent) {
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_NoSystemBackground);
}

void PreviewOverlay::setTitle(const QString& text, const QString& stylePreset) {
    text_ = text;
    stylePreset_ = stylePreset;
    visible_ = !text.isEmpty();
    update();
}

void PreviewOverlay::clearTitle() {
    text_.clear();
    stylePreset_.clear();
    visible_ = false;
    update();
}

void PreviewOverlay::paintEvent(QPaintEvent*) {
    if (!visible_ || text_.isEmpty()) return;

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const QRect r = rect().adjusted(24, 24, -24, -24);

    if (stylePreset_ == "quote") {
        QFont quoteFont = font();
        quoteFont.setPointSize(28);
        quoteFont.setBold(true);
        painter.setFont(quoteFont);
        painter.setPen(QColor(240, 240, 245));
        painter.drawText(r, Qt::AlignCenter, QString("\u201C") + text_ + QString("\u201D"));
        return;
    }

    if (stylePreset_ == "meme") {
        QFont memeFont = font();
        memeFont.setPointSize(18);
        memeFont.setBold(true);
        painter.setFont(memeFont);
        painter.setPen(Qt::white);

        QRect topBar(r.left(), r.top(), r.width(), 56);
        QRect bottomBar(r.left(), r.bottom() - 56, r.width(), 56);
        painter.fillRect(topBar, QColor(0, 0, 0, 190));
        painter.fillRect(bottomBar, QColor(0, 0, 0, 190));
        painter.drawText(topBar, Qt::AlignCenter, text_);
        painter.drawText(bottomBar, Qt::AlignCenter, text_);
        return;
    }

    if (stylePreset_ == "intro-clean" || stylePreset_ == "outro-stamp") {
        QFont titleFont = font();
        titleFont.setPointSize(36);
        titleFont.setBold(true);
        painter.setFont(titleFont);
        painter.setPen(QColor(245, 245, 250));
        painter.drawText(r, Qt::AlignCenter, text_);
        return;
    }

    if (stylePreset_ == "rating") {
        painter.setPen(QColor(255, 210, 80));
        QFont starFont = font();
        starFont.setPointSize(22);
        painter.setFont(starFont);
        painter.drawText(r.adjusted(0, 0, 0, -30), Qt::AlignHCenter | Qt::AlignBottom,
                         QString("★ ★ ★ ★ ★"));
        painter.setPen(Qt::white);
        painter.drawText(r.adjusted(0, 30, 0, 0), Qt::AlignHCenter | Qt::AlignTop, text_);
        return;
    }

    if (stylePreset_ == "credits") {
        QFont creditFont = font();
        creditFont.setPointSize(16);
        painter.setFont(creditFont);
        painter.setPen(QColor(230, 230, 235));
        painter.drawText(r, Qt::AlignHCenter | Qt::AlignBottom, text_);
        return;
    }

    if (stylePreset_ == "timer") {
        QFont timerFont = font();
        timerFont.setPointSize(42);
        timerFont.setBold(true);
        painter.setFont(timerFont);
        painter.setPen(QColor(255, 255, 255));
        painter.drawText(r, Qt::AlignCenter, text_);
        return;
    }

    // Default lower-third styles
    const int barHeight = stylePreset_ == "lower-third-broadcast" ? 72 : 52;
    QRect bar(r.left() + 24, r.bottom() - barHeight - 24, std::min(520, r.width() - 48), barHeight);

    QColor barColor = stylePreset_ == "lower-third-broadcast" ? QColor(30, 90, 200, 220)
                                                              : QColor(20, 20, 24, 200);
    painter.fillRect(bar, barColor);

    if (stylePreset_ == "lower-third-minimal") {
        painter.fillRect(bar.left(), bar.top(), 4, bar.height(), QColor(255, 200, 60));
    }

    QFont labelFont = font();
    labelFont.setPointSize(14);
    labelFont.setBold(true);
    painter.setFont(labelFont);
    painter.setPen(Qt::white);
    painter.drawText(bar.adjusted(16, 0, -12, 0), Qt::AlignVCenter | Qt::AlignLeft, text_);
}

} // namespace nova::ui
