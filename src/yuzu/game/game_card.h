// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QPixmap>
#include <QStyledItemDelegate>

/**
 * A stylized "card"-like delegate for the game grid view.
 * Adapted from QML
 */
class GameCard : public QStyledItemDelegate {
    Q_OBJECT
public:
    explicit GameCard(QObject* parent = nullptr);

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;

    QRect getCardRect(const QStyleOptionViewItem &option, const QModelIndex &index) const;

    bool hitTest(const QPoint& point, const QModelIndex& index,
                 const QWidget* widget, const QRect& cellRect) const;

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    void setSize(const QSize& newSize, const QSize& contentSize, const int padding, const int columns);
    void SetHomeMode(bool enabled) { home_mode = enabled; }
    void SetHomeSize(int height, int cover_width, int banner_width);
    void SetHomeBanner(QPixmap banner);
    static QString HomeTitle(const QModelIndex& index);
    static QPixmap LoadHomeArtwork(const QModelIndex& index, bool banner);
    static void DrawCroppedArtwork(QPainter& painter, const QRectF& rect, const QPixmap& artwork);

private:
    static constexpr int cardMargin = 8;
    void PaintHomeCard(QPainter* painter, const QStyleOptionViewItem& option,
                       const QModelIndex& index, const QRect& card) const;

    QSize m_size;
    QSize m_contentSize;
    QPixmap home_banner;
    int home_height{};
    int home_cover_width{};
    int home_banner_width{};
    int m_padding{};
    int m_columns{};
    bool home_mode{};
};
