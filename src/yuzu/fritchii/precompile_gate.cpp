// SPDX-License-Identifier: GPL-3.0-or-later
// Modified for Jinra: uses Jinra's shader preparation screen.
// Shader precompile system by Frizti (Fritchii)

#include "yuzu/fritchii/precompile_gate.h"


#include <QDialog>
#include <QEvent>
#include <QLabel>
#include <QMainWindow>
#include <QStatusBar>
#include <QTimer>
#include <QVBoxLayout>

#include "core/core.h"
#include "qt_common/qt_common.h"
#include "qt_common/render/emu_thread.h"
#include "video_core/gpu.h"
#include "video_core/rasterizer_interface.h"
#include "video_core/shader_notify.h"
#include "yuzu/loading_screen.h"

namespace FritchiiUi {
namespace {

// A check that is over this quickly (prepared already, nothing to prepare) never shows the
// screen: the game only waits that long.
constexpr qint64 QuietCheckMs = 800;
// The rate behind "time left" comes from the last half minute.
constexpr qint64 RateWindowMs = 30'000;

VideoCore::ShaderNotify* Notify() {
    if (!QtCommon::system || !QtCommon::system->IsPoweredOn()) {
        return nullptr;
    }
    return &QtCommon::system->GPU().ShaderNotify();
}

} // namespace

PrecompileGate::PrecompileGate(QMainWindow* window_, QStatusBar* status_bar_,
                               PrecompileHooks hooks_)
    : QObject(window_), window{window_}, status_bar{status_bar_}, hooks{std::move(hooks_)} {
    timer = new QTimer(this);
    timer->setInterval(250);
    connect(timer, &QTimer::timeout, this, &PrecompileGate::Poll);

    status_note = new QLabel;
    status_note->setObjectName(QStringLiteral("FritchiiStatusNote"));
    status_note->hide();
    status_bar->addPermanentWidget(status_note);

    window->installEventFilter(this);
    clock.start();
}

PrecompileGate::~PrecompileGate() = default;

void PrecompileGate::OnEmulationStarting() {
    gated_this_boot = false;
    phase = Phase::Off;
    if (QtCommon::emu_thread) {
        connect(
            QtCommon::emu_thread.get(), &EmuThread::LoadProgress, this,
            [this](VideoCore::LoadCallbackStage stage, std::size_t, std::size_t) {
                if (stage == VideoCore::LoadCallbackStage::Complete) {
                    OnLoadComplete();
                }
            },
            Qt::QueuedConnection);
    }
}

void PrecompileGate::OnEmulationStopping() {
    CloseScreen();
    timer->stop();
    phase = Phase::Off;
    status_note->hide();
}

void PrecompileGate::OnLoadComplete() {
    // Once per boot: a later shader cache reload says Complete too.
    if (gated_this_boot) {
        return;
    }
    gated_this_boot = true;
    auto* notify = Notify();
    if (!notify || !notify->PrecompilePending()) {
        return;
    }
    // Hold the game while the preparation decides; a quick "already prepared" lets it go at once.
    hooks.pause();
    phase = Phase::Checking;
    samples.clear();
    since_check.start();
    timer->start();
}

void PrecompileGate::Poll() {
    auto* notify = Notify();
    if (!notify) {
        OnEmulationStopping();
        return;
    }
    const bool pending = notify->PrecompilePending();
    const auto progress = notify->GetPrecompileProgress();
    const bool counting = progress.total > 0 && progress.done < progress.total;
    if (counting) {
        samples.emplace_back(clock.elapsed(), progress.done);
        while (samples.size() > 2 && samples.back().first - samples.front().first > RateWindowMs) {
            samples.pop_front();
        }
    }

    switch (phase) {
    case Phase::Checking:
        if (!pending) {
            phase = Phase::Off;
            timer->stop();
            hooks.resume();
        } else if (counting || since_check.elapsed() > QuietCheckMs) {
            ShowScreen();
            loading_view->ShowShaderPreparationProgress(progress.done, progress.total);
        }
        break;
    case Phase::Screen:
        if (!pending) {
            CloseScreen();
            phase = Phase::Off;
            timer->stop();
            hooks.resume();
        } else {
            loading_view->ShowShaderPreparationProgress(progress.done, progress.total);
        }
        break;
    case Phase::Background:
        if (!pending) {
            status_note->hide();
            phase = Phase::Off;
            timer->stop();
        } else if (counting) {
            const int percent = static_cast<int>(progress.done * 100 / progress.total);
            // With the time left at the last half minute's rate, when it moves (on Intel it
            // only builds while paused).
            const QString left = TimeLeft(progress.done, progress.total);
            status_note->setText(left.isEmpty()
                                     ? tr("Preparing shaders %1%").arg(percent)
                                     : tr("Preparing shaders %1% · %2").arg(percent).arg(left));
            status_note->show();
        }
        break;
    case Phase::Off:
        timer->stop();
        break;
    }
}

void PrecompileGate::ShowScreen() {
    phase = Phase::Screen;
    if (auto* notify = Notify()) {
        notify->SetPrecompileFullSpeed(true);
    }
    if (screen) {
        screen->show();
        return;
    }
    screen = new QDialog(window, Qt::FramelessWindowHint | Qt::Dialog);
    screen->setModal(true);
    // Esc or Alt+F4 on the screen: play now (the preparation goes on in the background).
    connect(screen, &QDialog::rejected, this, &PrecompileGate::PlayNow);

    auto* outer = new QVBoxLayout(screen);
    outer->setContentsMargins(0, 0, 0, 0);
    loading_view = new LoadingScreen(screen);
    outer->addWidget(loading_view);
    connect(loading_view, &LoadingScreen::PlayNowRequested, this, &PrecompileGate::PlayNow);

    PlaceScreen();
    screen->show();
    screen->raise();
    screen->activateWindow();
}

QString PrecompileGate::TimeLeft(size_t done, size_t total) {
    if (samples.size() < 2) {
        return {};
    }
    const auto& [first_ms, first_done] = samples.front();
    const auto& [last_ms, last_done] = samples.back();
    const qint64 span = last_ms - first_ms;
    if (span < 3000 || last_done <= first_done) {
        return {};
    }
    const double per_ms = static_cast<double>(last_done - first_done) / static_cast<double>(span);
    const double seconds = static_cast<double>(total - done) / per_ms / 1000.0;
    if (seconds < 60) {
        return tr("Less than a minute left");
    }
    const int minutes = static_cast<int>(seconds / 60 + 0.5);
    if (minutes < 60) {
        return tr("About %n min left", "", minutes);
    }
    return tr("About %1 h %2 min left").arg(minutes / 60).arg(minutes % 60);
}

void PrecompileGate::PlayNow() {
    if (phase != Phase::Screen) {
        return;
    }
    CloseScreen();
    phase = Phase::Background;
    hooks.resume();
}

void PrecompileGate::CloseScreen() {
    if (auto* notify = Notify()) {
        notify->SetPrecompileFullSpeed(false);
    }
    if (screen) {
        QDialog* closing = screen;
        screen = nullptr;
        loading_view = nullptr;
        closing->hide();
        closing->deleteLater();
    }
}

void PrecompileGate::PlaceScreen() {
    if (!screen) {
        return;
    }
    QWidget* area = window->centralWidget();
    const QRect rect(area->mapToGlobal(QPoint(0, 0)), area->size());
    screen->setGeometry(rect);
}

bool PrecompileGate::eventFilter(QObject* watched, QEvent* event) {
    if (watched == window && screen &&
        (event->type() == QEvent::Move || event->type() == QEvent::Resize)) {
        PlaceScreen();
    }
    return QObject::eventFilter(watched, event);
}

} // namespace FritchiiUi
