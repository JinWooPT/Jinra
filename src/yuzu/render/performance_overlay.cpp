// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "performance_overlay.h"

#include <algorithm>
#include <cmath>
#include <utility>

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#elif defined(__linux__)
#include <fstream>
#include <unistd.h>
#elif defined(__APPLE__)
#include <mach/mach.h>
#endif

#include <QFontMetrics>
#include <QGuiApplication>
#include <QLocale>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QWindow>

#include "core/perf_stats.h"
#include "main_window.h"

namespace {
constexpr int Padding = 5;
constexpr int LineSpacing = 3;

QFont ReadingFont(QFont font, bool primary) {
    font.setPixelSize(primary ? 23 : 17);
    font.setWeight(primary ? QFont::Bold : QFont::DemiBold);
    return font;
}

std::optional<u64> ProcessResidentMemory() {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS counters{};
    counters.cb = sizeof(counters);
    if (K32GetProcessMemoryInfo(GetCurrentProcess(), &counters, counters.cb)) {
        return counters.WorkingSetSize;
    }
#elif defined(__linux__)
    std::ifstream memory("/proc/self/statm");
    u64 virtual_pages{}, resident_pages{};
    const auto page_size = sysconf(_SC_PAGESIZE);
    if (page_size > 0 && memory >> virtual_pages >> resident_pages) {
        return resident_pages * static_cast<u64>(page_size);
    }
#elif defined(__APPLE__)
    mach_task_basic_info_data_t info{};
    mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO,
                  reinterpret_cast<task_info_t>(&info), &count) == KERN_SUCCESS) {
        return info.resident_size;
    }
#endif
    return std::nullopt;
}
} // namespace

PerformanceOverlay::PerformanceOverlay(MainWindow* parent)
    : QWidget(parent, Qt::FramelessWindowHint | Qt::Tool), main_window(parent) {
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setReadings({tr("— FPS"), tr("— ms"), tr("RAM —"), tr("VRAM —"), {}});
    resetPosition();
    connect(parent, &MainWindow::positionChanged, this, &PerformanceOverlay::resetPosition);
}

void PerformanceOverlay::resetPosition() {
    move(main_window->pos() + offset);
}

void PerformanceOverlay::UpdateStats(const Core::PerfStatsResults& results, int shaders_building,
                                     std::optional<VideoCore::VideoMemoryUsage> video_memory) {
    const QLocale locale;
    const auto format_bytes = [&locale](u64 bytes) {
        return locale.formattedDataSize(static_cast<qint64>(bytes), 1, QLocale::DataSizeIecFormat);
    };
    const auto ram = ProcessResidentMemory();
    QString vram = tr("VRAM —");
    if (video_memory) {
        const auto size = format_bytes(video_memory->bytes);
        vram = video_memory->shared ? tr("GPU memory (shared) %1").arg(size)
                                    : tr("VRAM %1").arg(size);
    }
    setReadings({
        std::isfinite(results.average_game_fps)
            ? tr("%1 FPS").arg(locale.toString(results.average_game_fps, 'f', 0))
            : tr("— FPS"),
        std::isfinite(results.frametime)
            ? tr("%1 ms").arg(locale.toString(results.frametime * 1000.0, 'f', 2))
            : tr("— ms"),
        ram ? tr("RAM %1").arg(format_bytes(*ram)) : tr("RAM —"),
        std::move(vram),
        shaders_building > 0 ? tr("Shaders building: %1").arg(locale.toString(shaders_building))
                             : QString{},
    });
}

void PerformanceOverlay::setReadings(std::array<QString, 5> next) {
    if (readings == next) {
        return;
    }
    readings = std::move(next);
    int width = 0;
    int height = 2 * Padding;
    for (std::size_t i = 0; i < readings.size() && !readings[i].isEmpty(); ++i) {
        const QFontMetrics metrics(ReadingFont(font(), i == 0));
        width = std::max(width, metrics.horizontalAdvance(readings[i]));
        height += metrics.height() + LineSpacing;
    }
    setFixedSize(width + 2 * Padding, height - LineSpacing);
    update();
}

void PerformanceOverlay::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const QPen outline(QColor(4, 9, 17, 245), 3.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    int y = Padding;
    for (std::size_t i = 0; i < readings.size() && !readings[i].isEmpty(); ++i) {
        const auto reading_font = ReadingFont(font(), i == 0);
        const QFontMetrics metrics(reading_font);
        QPainterPath text;
        text.addText(Padding, y + metrics.ascent(), reading_font, readings[i]);
        painter.strokePath(text, outline);
        painter.fillPath(text, i == 4 ? QColor("#71e5f2") : QColor("#f5faff"));
        y += metrics.height() + LineSpacing;
    }
}

void PerformanceOverlay::mousePressEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton) {
        return;
    }
    if (QGuiApplication::platformName().startsWith(QStringLiteral("wayland")) && windowHandle() &&
        windowHandle()->startSystemMove()) {
        return;
    }
    drag_start = event->pos();
}

void PerformanceOverlay::mouseMoveEvent(QMouseEvent* event) {
    if (event->buttons() & Qt::LeftButton) {
        const auto position = event->globalPosition().toPoint() - drag_start;
        offset = position - main_window->pos();
        move(position);
    }
}

void PerformanceOverlay::closeEvent(QCloseEvent*) {
    emit closed();
}
