// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <array>
#include <optional>

#include <QWidget>

#include "video_core/renderer_base.h"

namespace Core {
struct PerfStatsResults;
}
class MainWindow;

class PerformanceOverlay : public QWidget {
    Q_OBJECT

public:
    explicit PerformanceOverlay(MainWindow* parent);
    void UpdateStats(const Core::PerfStatsResults& results, int shaders_building,
                     std::optional<VideoCore::VideoMemoryUsage> video_memory);

signals:
    void closed();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

private:
    void resetPosition();
    void setReadings(std::array<QString, 5> readings);

    MainWindow* main_window;
    std::array<QString, 5> readings;
    QPoint offset{25, 75};
    QPoint drag_start;
};
