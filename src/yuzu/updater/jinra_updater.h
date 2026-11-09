// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <functional>
#include <QNetworkAccessManager>
#include <QObject>
#include <QPointer>
#include "yuzu/updater/jinra_release.h"

class QAction;
class QNetworkReply;
class QProgressDialog;
class QWidget;

class JinraUpdater final : public QObject {
public:
    JinraUpdater(QWidget* parent, QAction* action, std::function<bool()> can_install,
                 std::function<void()> exit_for_install);
    ~JinraUpdater() override;
    void Check(bool manual);

private:
    void Offer(const Jinra::UpdateRelease& release);
    void Download(const Jinra::UpdateRelease& release);
    void DownloadAttempt(const Jinra::UpdateRelease& release, const QString& filename, int attempt);
    void FinishDownload();
    QWidget* window;
    QAction* action;
    std::function<bool()> can_install;
    std::function<void()> exit_for_install;
    QNetworkAccessManager network;
    QPointer<QNetworkReply> active;
    QPointer<QProgressDialog> download_progress;
};
