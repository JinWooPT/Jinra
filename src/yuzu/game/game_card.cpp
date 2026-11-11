// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QAbstractItemView>
#include <QFileInfo>
#include <QPainter>
#include <QPainterPath>
#include <QTextLayout>
#include "game_card.h"
#include "qt_common/config/uisettings.h"
#include "qt_common/game_list/game_list_p.h"

GameCard::GameCard(QObject* parent) : QStyledItemDelegate{parent} {
    setObjectName("GameCard");
}

void GameCard::paint(QPainter* painter, const QStyleOptionViewItem& option,
                     const QModelIndex& index) const {
    if (!index.isValid())
        return;

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);

    constexpr int cardCornerRadius = 10;

    const QRect cardRect = getCardRect(option, index);
    if (home_mode) {
        PaintHomeCard(painter, option, index, cardRect);
        painter->restore();
        return;
    }

    QPalette palette = option.palette;
    QColor backgroundColor = QColor("#111b2c");
    QColor borderColor = QColor("#24344c");
    QColor textColor = QColor("#e8f1fc");

    // highlight on select or hover
    if (option.state & QStyle::State_Selected) {
        backgroundColor = QColor("#162f4c");
        borderColor = QColor("#278dff");
        textColor = QColor("#ffffff");
    } else if (option.state & QStyle::State_MouseOver) {
        backgroundColor = backgroundColor.lighter(120);
    }

    painter->setBrush(backgroundColor);
    painter->setPen(QPen(borderColor, 1));
    painter->drawRoundedRect(cardRect, cardCornerRadius, cardCornerRadius);

    const u32 icon_size = UISettings::values.game_icon_size.GetValue();
    QPixmap icon_pixmap = index.data(Qt::DecorationRole).value<QPixmap>();

    QRect iconRect;
    if (!icon_pixmap.isNull()) {
        QSize scaled = icon_pixmap.size();
        scaled.scale(icon_size, icon_size, Qt::KeepAspectRatio);

        iconRect = {cardRect.left() + (cardRect.width() - scaled.width()) / 2,
                    cardRect.top() + cardMargin - 1, scaled.width(), scaled.height()};

        painter->setRenderHint(QPainter::SmoothPixmapTransform, true);

        painter->save();
        QPainterPath clip_path;
        clip_path.addRoundedRect(iconRect, cardCornerRadius, cardCornerRadius);
        painter->setClipPath(clip_path);
        painter->drawPixmap(iconRect, icon_pixmap);
        painter->restore();
    } else {
        iconRect = {cardRect.left() + cardMargin, cardRect.top() + cardMargin,
                    static_cast<int>(icon_size), static_cast<int>(icon_size)};
    }

    if (UISettings::values.show_game_name.GetValue()) {
        QRect textRect = cardRect;
        textRect.setTop(iconRect.bottom() + cardMargin);
        textRect.adjust(cardMargin, 0, -cardMargin, -cardMargin);

        QString title = index.data(Qt::DisplayRole).toString().split(QLatin1Char('\n')).first();

        painter->setPen(textColor);
        QFont font = option.font;
        font.setBold(true);
        font.setPixelSize(std::max(11.0, std::sqrt(static_cast<double>(icon_size))));
        painter->setFont(font);

        painter->drawText(textRect, Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap, title);
    }

    painter->restore();
}

void GameCard::PaintHomeCard(QPainter* painter, const QStyleOptionViewItem& option,
                             const QModelIndex& index, const QRect& card) const {
    const auto* view = qobject_cast<const QAbstractItemView*>(option.widget);
    const bool selected = (option.state & QStyle::State_Selected) ||
                          (view && view->currentIndex() == index);
    QPainterPath shape;
    shape.addRoundedRect(card, 13, 13);
    painter->save();
    painter->setClipPath(shape);
    painter->fillRect(card, selected ? QColor("#162f4c") : QColor("#141e30"));
    const QRect artwork{card.topLeft(), QSize{card.width(), card.width()}};
    auto image = index.data(Qt::DecorationRole).value<QPixmap>();
    if (image.isNull()) {
        painter->fillRect(artwork, QColor("#111a2a"));
        image = QPixmap(QStringLiteral(":/jinra/icon.png"));
        const int edge = artwork.width() / 3;
        QRect fallback(0, 0, edge, edge);
        fallback.moveCenter(artwork.center());
        painter->setRenderHint(QPainter::SmoothPixmapTransform);
        painter->drawPixmap(fallback, image, image.rect());
    } else {
        const int edge = std::min(image.width(), image.height());
        const QRect crop((image.width() - edge) / 2, (image.height() - edge) / 2, edge, edge);
        painter->setRenderHint(QPainter::SmoothPixmapTransform);
        painter->drawPixmap(artwork, image, crop);
    }

    const int left = card.left() + 12;
    const int text_width = card.width() - 24;
    auto title = index.data(GameListItemPath::TitleRole).toString();
    bool numeric_title{};
    title.toULongLong(&numeric_title);
    if (title.isEmpty() || numeric_title) {
        title = QFileInfo(index.data(GameListItemPath::FullPathRole).toString()).completeBaseName();
    }
    QFont font = option.font;
    font.setPixelSize(std::clamp(card.width() / 14, 14, 22));
    font.setWeight(QFont::DemiBold);
    QTextLayout lines(title, font);
    lines.beginLayout();
    const auto first = lines.createLine();
    painter->setFont(font);
    painter->setPen(QColor("#e8f1fc"));
    const int baseline = artwork.bottom() + 9 + painter->fontMetrics().ascent();
    if (first.isValid()) {
        auto first_line = first;
        first_line.setLineWidth(text_width);
        const int split = first_line.textLength();
        painter->drawText(left, baseline, title.left(split));
        painter->drawText(left, baseline + painter->fontMetrics().height(),
                          painter->fontMetrics().elidedText(title.mid(split).trimmed(),
                                                           Qt::ElideRight, text_width));
    }
    lines.endLayout();
    painter->restore();

    painter->setBrush(Qt::NoBrush);
    painter->setPen(QPen(selected ? QColor("#35d9ed") : QColor("#283a54"), selected ? 3 : 1));
    const int ring = selected ? 4 : 0;
    painter->drawRoundedRect(card.adjusted(-ring, -ring, ring, ring), 13 + ring, 13 + ring);
}

QRect GameCard::getCardRect(const QStyleOptionViewItem& option, const QModelIndex& index) const {
    if (home_mode) {
        return {option.rect.left() + 18,
                option.rect.top() + (option.rect.height() - m_contentSize.height()) / 2,
                m_contentSize.width(), m_contentSize.height()};
    }
    const int cell_width = option.rect.width();

    const int card_width = cell_width - m_padding;

    int card_left, card_top, card_height;

    if (m_columns >= 1) {
        // grid mode

        // center everything in-line, such that the leftmost and rightmost cards
        // have ~ equal padding to the edge of the viewport
        // spacing between each card is larger, but equal to each other
        const int column = index.row() % m_columns;

        const int row_width = m_columns * cell_width;
        const int total_gap = row_width - cardMargin * 2 - m_columns * card_width;
        const int gap = (m_columns > 1) ? (total_gap / (m_columns - 1)) : 0;
        card_left =
            option.rect.left() - column * cell_width + cardMargin + column * (card_width + gap) + 4;

        // fill cell vertically
        card_top = option.rect.top() + cardMargin;
        card_height = option.rect.height() - cardMargin - 1;
    } else {
        // carousel mode
        card_left = option.rect.left() + cardMargin + 4;

        // the delegate itself takes up the full height, but the card itself
        // gets centered
        const int content_height = m_contentSize.height() - cardMargin;
        const int cell_height = option.rect.height();

        card_height = std::min(content_height, cell_height - cardMargin * 2) - 1;
        card_top = option.rect.top() + (cell_height - card_height) / 2;
    }

    return QRect(card_left, card_top, card_width - cardMargin, card_height);
}

bool GameCard::hitTest(const QPoint& point, const QModelIndex& index, const QWidget* widget,
                       const QRect& cellRect) const {
    QStyleOptionViewItem option;
    option.initFrom(widget);
    option.rect = cellRect;
    return getCardRect(option, index).contains(point);
}

QSize GameCard::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const {
    return m_size;
}

void GameCard::setSize(const QSize& newSize, const QSize& contentSize, const int padding,
                       const int columns) {
    m_size = newSize;
    m_contentSize = contentSize;
    m_padding = padding;
    m_columns = columns;
}
