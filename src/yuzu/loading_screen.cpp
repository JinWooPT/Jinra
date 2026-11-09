// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2019 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <limits>
#include <QBuffer>
#include <QByteArray>
#include <QGraphicsOpacityEffect>
#include <QIODevice>
#include <QImage>
#include <QLocale>
#include <QPainter>
#include <QPixmap>
#include <QPropertyAnimation>
#include <QStyleOption>
#include "core/frontend/framebuffer_layout.h"
#include "core/loader/loader.h"
#include "ui_loading_screen.h"
#include "video_core/rasterizer_interface.h"
#include "yuzu/loading_screen.h"

// Mingw seems to not have QMovie at all. If QMovie is missing then use a single frame instead of an
// showing the full animation
#if !YUZU_QT_MOVIE_MISSING
#include <QMovie>
#endif

LoadingScreen::LoadingScreen(QWidget* parent)
    : QWidget(parent), ui(std::make_unique<Ui::LoadingScreen>()),
      previous_stage(VideoCore::LoadCallbackStage::Complete) {
    ui->setupUi(this);
    setMinimumSize(Layout::MinimumSize::Width, Layout::MinimumSize::Height);
    const QPixmap icon{QStringLiteral(":/jinra/icon.png")};
    ui->brandIcon->setPixmap(icon.scaled(44, 44, Qt::KeepAspectRatio, Qt::SmoothTransformation));

    // Fade the content while retaining the loading screen's dark background.
    opacity_effect = new QGraphicsOpacityEffect(this);
    opacity_effect->setOpacity(1);
    ui->fade_parent->setGraphicsEffect(opacity_effect);
    fadeout_animation = std::make_unique<QPropertyAnimation>(opacity_effect, "opacity");
    fadeout_animation->setDuration(500);
    fadeout_animation->setStartValue(1);
    fadeout_animation->setEndValue(0);
    fadeout_animation->setEasingCurve(QEasingCurve::OutBack);

    // After the fade completes, hide the widget and reset the opacity
    connect(fadeout_animation.get(), &QPropertyAnimation::finished, [this] {
        hide();
        opacity_effect->setOpacity(1);
        emit Hidden();
    });
    connect(this, &LoadingScreen::LoadProgress, this, &LoadingScreen::OnLoadProgress,
            Qt::QueuedConnection);
    connect(ui->playNow, &QPushButton::clicked, this, [this] {
        play_now_requested = true;
        ui->playNow->setEnabled(false);
        ui->preparationNote->setText(
            tr("Shaders will continue compiling in the background. You may notice brief stutters."));
        emit PlayNowRequested();
    });
    qRegisterMetaType<VideoCore::LoadCallbackStage>();
}

LoadingScreen::~LoadingScreen() = default;

void LoadingScreen::Prepare(Loader::AppLoader& loader) {
    play_now_requested = false;
    ui->playNow->setEnabled(true);
    ui->banner->clear();
    ui->logo->clear();
    std::vector<u8> buffer;
    if (loader.ReadBanner(buffer) == Loader::ResultStatus::Success) {
#ifdef YUZU_QT_MOVIE_MISSING
        QPixmap map;
        map.loadFromData(buffer.data(), buffer.size());
        ui->banner->setPixmap(map);
#else
        backing_mem = std::make_unique<QByteArray>(reinterpret_cast<char*>(buffer.data()),
                                                   static_cast<int>(buffer.size()));
        backing_buf = std::make_unique<QBuffer>(backing_mem.get());
        backing_buf->open(QIODevice::ReadOnly);
        animation = std::make_unique<QMovie>(backing_buf.get(), QByteArray());
        animation->start();
        ui->banner->setMovie(animation.get());
#endif
        buffer.clear();
    }
    if (loader.ReadLogo(buffer) == Loader::ResultStatus::Success) {
        QPixmap map;
        map.loadFromData(buffer.data(), static_cast<uint>(buffer.size()));
        ui->logo->setPixmap(map);
    }

    OnLoadProgress(VideoCore::LoadCallbackStage::Prepare, 0, 0);
}

void LoadingScreen::OnLoadComplete() {
    fadeout_animation->start(QPropertyAnimation::KeepWhenStopped);
}

void LoadingScreen::OnLoadProgress(VideoCore::LoadCallbackStage stage, std::size_t value,
                                   std::size_t total) {
    using namespace std::chrono;
    using Stage = VideoCore::LoadCallbackStage;
    const auto now = steady_clock::now();
    if (stage != previous_stage || total != previous_total || value < first_value) {
        stage_start = now;
        first_value = value;
        previous_stage = stage;
        previous_total = total;
    }

    switch (stage) {
    case Stage::Prepare:
        ui->stage->setText(tr("Loading game…"));
        break;
    case Stage::Build:
        ui->stage->setText(tr("Loading shaders"));
        break;
    case Stage::Complete:
        ui->stage->setText(tr("Launching game…"));
        break;
    case Stage::PrepareShaders:
        ui->stage->setText(tr("Preparing shaders"));
        break;
    }
    if (!play_now_requested) {
        ui->preparationNote->setText(tr("This happens once, so the game runs smoothly"));
    }
    ui->preparationNote->setVisible(stage == Stage::PrepareShaders || play_now_requested);
    ui->playNow->setVisible(stage == Stage::PrepareShaders && value < total);

    const bool show_progress = total > 0 && (stage == Stage::Build || stage == Stage::PrepareShaders);
    ui->progressDetails->setVisible(show_progress);
    if (!show_progress) {
        return;
    }

    const auto done = std::min(value, total);
    const double fraction = static_cast<double>(done) / total;
    ui->progress_bar->setValue(static_cast<int>(fraction * ui->progress_bar->maximum()));
    const QLocale locale;
    ui->progressCount->setText(tr("%1 / %2")
                                   .arg(locale.toString(static_cast<qulonglong>(done)),
                                        locale.toString(static_cast<qulonglong>(total))));
    ui->progressPercent->setText(locale.toString(static_cast<int>(fraction * 100)) + locale.percent());

    QString estimate;
    const auto elapsed = duration<double>(now - stage_start).count();
    if (elapsed >= 2.0 && done > first_value && done < total) {
        const double seconds_left = elapsed * (total - done) / (done - first_value);
        const auto remaining = static_cast<qint64>(
            std::clamp(seconds_left, 1.0, static_cast<double>(std::numeric_limits<int>::max())));
        const auto hours = remaining / 3600;
        const auto minutes = remaining / 60 % 60;
        const auto seconds = remaining % 60;
        const QString duration_text = hours > 0 ? tr("%1 h %2 min").arg(hours).arg(minutes)
                                                : tr("%1 min %2 s").arg(minutes).arg(seconds);
        estimate = tr("About %1 left").arg(duration_text);
    }
    ui->value->setText(estimate);
}

void LoadingScreen::paintEvent(QPaintEvent* event) {
    QStyleOption opt;
    opt.initFrom(this);
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
    QWidget::paintEvent(event);
}

void LoadingScreen::Clear() {
#ifndef YUZU_QT_MOVIE_MISSING
    animation.reset();
    backing_buf.reset();
    backing_mem.reset();
#endif
}
