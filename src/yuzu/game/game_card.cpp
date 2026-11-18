// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <algorithm>
#include <utility>
#include <QAbstractItemView>
#include <QDateTime>
#include <QFileInfo>
#include <QImageReader>
#include <QPainter>
#include <QPainterPath>
#include <QPixmapCache>
#include <QTextLayout>
#include "game_card.h"
#include "common/fs/path_util.h"
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
    const auto* view = static_cast<const QAbstractItemView*>(parent());
    const bool selected = view->currentIndex() == index;
    const bool has_banner = selected && !home_banner.isNull();
    const auto image = has_banner ? home_banner : index.data(Qt::DecorationRole).value<QPixmap>();
    QPainterPath shape;
    shape.addRoundedRect(card, 12, 12);
    painter->save();
    painter->setClipPath(shape);
    painter->fillRect(card, QColor("#141e30"));
    DrawCroppedArtwork(*painter, card, image);

    if (!has_banner) {
        QLinearGradient shade(card.topLeft(), card.bottomLeft());
        shade.setColorAt(0.46, Qt::transparent);
        shade.setColorAt(1.0, QColor(8, 13, 24, 247));
        painter->fillRect(card, shade);
        QFont font = option.font;
        font.setPixelSize(std::clamp(card.height() / 13, 12, 19));
        font.setWeight(QFont::DemiBold);
        painter->setFont(font);
        painter->setPen(QColor("#f3f8ff"));
        const auto metrics = painter->fontMetrics();
        const int text_width = card.width() - 22;
        const int baseline = card.bottom() - 10 - metrics.descent();
        const auto title = HomeTitle(index);
        QTextLayout lines(title, font);
        lines.beginLayout();
        auto first = lines.createLine();
        if (first.isValid()) {
            first.setLineWidth(text_width);
            const auto split = first.textLength();
            const auto rest = title.mid(split).trimmed();
            painter->drawText(card.left() + 11, baseline - (rest.isEmpty() ? 0 : metrics.height()),
                              metrics.elidedText(title.left(split).trimmed(), Qt::ElideRight,
                                                 text_width));
            if (!rest.isEmpty()) {
                painter->drawText(card.left() + 11, baseline,
                                  metrics.elidedText(rest, Qt::ElideRight, text_width));
            }
        }
        lines.endLayout();
    }
    painter->restore();

    painter->setBrush(Qt::NoBrush);
    painter->setPen(QPen(selected ? QColor("#35d9ed") : QColor("#344157"), selected ? 2 : 1));
    const int ring = selected ? 3 : 0;
    painter->drawRoundedRect(card.adjusted(-ring, -ring, ring, ring), 12 + ring, 12 + ring);
}

QRect GameCard::getCardRect(const QStyleOptionViewItem& option, const QModelIndex& index) const {
    if (home_mode) {
        const auto* view = static_cast<const QAbstractItemView*>(parent());
        const bool show_banner = view->currentIndex() == index && !home_banner.isNull();
        const int width = show_banner
                              ? std::clamp(qRound(qreal(home_height) * home_banner.width() /
                                                   home_banner.height()),
                                           home_cover_width, home_banner_width)
                              : home_cover_width;
        return {option.rect.left() + 5, option.rect.top() + 5, width, home_height};
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
    if (home_mode) {
        const auto card = getCardRect(option, index);
        return {card.width() + 17, card.height() + 10};
    }
    return m_size;
}

void GameCard::SetHomeSize(int height, int cover_width, int banner_width) {
    home_height = height;
    home_cover_width = cover_width;
    home_banner_width = banner_width;
}

void GameCard::SetHomeBanner(QPixmap banner) {
    home_banner = std::move(banner);
}

QString GameCard::HomeTitle(const QModelIndex& index) {
    const auto title = index.data(GameListItemPath::TitleRole).toString();
    bool numeric{};
    title.toULongLong(&numeric);
    return title.isEmpty() || numeric
               ? QFileInfo(index.data(GameListItemPath::FullPathRole).toString()).completeBaseName()
               : title;
}

QPixmap GameCard::LoadHomeArtwork(const QModelIndex& index, bool banner) {
    const auto title = index.data(GameListItemPath::ProgramIdRole).toULongLong();
    if (!title) {
        return {};
    }
    const auto directory = Common::FS::GetEdenPath(Common::FS::EdenPath::EdenDir) / "artwork";
    const auto id = QStringLiteral("%1").arg(title, 16, 16, QLatin1Char('0')).toUpper();
    const auto stem = QStringLiteral("%1/%2/%3")
                          .arg(QString::fromStdU16String(directory.u16string()), id,
                               banner ? QStringLiteral("banner") : QStringLiteral("background"));
    for (const auto& extension : {QStringLiteral(".jpg"), QStringLiteral(".png")}) {
        const QString path = stem + extension;
        const QFileInfo file(path);
        if (!file.isFile()) {
            continue;
        }
        const auto key = path + QString::number(file.lastModified().toMSecsSinceEpoch());
        QPixmap artwork;
        if (QPixmapCache::find(key, &artwork)) {
            return artwork;
        }
        QImageReader reader(path);
        reader.setAutoTransform(true);
        const QSize size = reader.size();
        if (!size.isValid()) {
            continue;
        }
        const QSize limit = banner ? QSize(920, 540) : QSize(1600, 1000);
        if (size.width() > limit.width() || size.height() > limit.height()) {
            reader.setScaledSize(size.scaled(limit, Qt::KeepAspectRatio));
        }
        artwork = QPixmap::fromImage(reader.read());
        if (!artwork.isNull()) {
            QPixmapCache::insert(key, artwork);
            return artwork;
        }
    }
    return QPixmap(QStringLiteral(":/jinra/banners/%1.jpg").arg(id));
}

void GameCard::DrawCroppedArtwork(QPainter& painter, const QRectF& rect, const QPixmap& artwork) {
    if (artwork.isNull() || rect.isEmpty()) {
        return;
    }
    const auto scale = std::max(rect.width() / artwork.width(), rect.height() / artwork.height());
    const QSizeF extent(rect.width() / scale, rect.height() / scale);
    const QRectF crop((artwork.width() - extent.width()) / 2,
                      (artwork.height() - extent.height()) / 2, extent.width(), extent.height());
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.drawPixmap(rect, artwork, crop);
}

void GameCard::setSize(const QSize& newSize, const QSize& contentSize, const int padding,
                       const int columns) {
    m_size = newSize;
    m_contentSize = contentSize;
    m_padding = padding;
    m_columns = columns;
}
