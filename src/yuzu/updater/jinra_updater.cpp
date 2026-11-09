// SPDX-License-Identifier: GPL-3.0-or-later
#include <algorithm>
#include <memory>
#include <QAction>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QJsonDocument>
#include <QMessageBox>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QProgressDialog>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTimer>
#include "yuzu/jinra_build_version.h"
#include "yuzu/updater/jinra_updater.h"

namespace {
QNetworkRequest Request(const QUrl& url) {
    QNetworkRequest request(url);
    request.setRawHeader("User-Agent", QByteArray("Jinra/") + Jinra::CurrentVersion);
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setMaximumRedirectsAllowed(5);
    request.setTransferTimeout(30000);
    return request;
}

struct DownloadState {
    explicit DownloadState(const QString& path) : file(path), hash(QCryptographicHash::Sha256) {}
    QSaveFile file;
    QCryptographicHash hash;
    qint64 bytes{};
    bool valid{true};
};

constexpr int download_attempts = 3;

bool IsTransientDownloadError(const QNetworkReply& reply) {
    const int status = reply.attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (status == 429 || (status >= 500 && status <= 599)) {
        return true;
    }
    switch (reply.error()) {
    case QNetworkReply::ConnectionRefusedError:
    case QNetworkReply::RemoteHostClosedError:
    case QNetworkReply::HostNotFoundError:
    case QNetworkReply::TimeoutError:
    case QNetworkReply::TemporaryNetworkFailureError:
    case QNetworkReply::NetworkSessionFailedError:
        return true;
    default:
        return false;
    }
}
} // namespace

JinraUpdater::JinraUpdater(QWidget* parent, QAction* action_, std::function<bool()> can_install_,
                           std::function<void()> exit_for_install_)
    : QObject(parent), window(parent), action(action_), can_install(std::move(can_install_)),
      exit_for_install(std::move(exit_for_install_)), network(this) {
    connect(action, &QAction::triggered, this, [this] { Check(true); });
}

JinraUpdater::~JinraUpdater() {
    if (active) {
        active->disconnect(this);
        active->abort();
    }
}

void JinraUpdater::Check(bool manual) {
#ifndef _WIN32
    if (manual)
        QMessageBox::information(window, tr("Jinra updates"),
                                 tr("Installer updates are currently available for Windows only."));
    return;
#endif
    if (active || download_progress)
        return;
    action->setEnabled(false);
    auto* reply = network.get(Request(
        QUrl(QStringLiteral("https://api.github.com/repos/JinWooPT/Jinra/releases?per_page=20"))));
    reply->setReadBufferSize(2 * 1024 * 1024 + 1);
    active = reply;
    connect(reply, &QNetworkReply::readyRead, this, [reply] {
        if (reply->bytesAvailable() > 2 * 1024 * 1024)
            reply->abort();
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply, manual] {
        active = nullptr;
        action->setEnabled(true);
        reply->deleteLater();
        const auto body = reply->readAll();
        QJsonParseError error;
        const auto json = QJsonDocument::fromJson(body, &error);
        if (reply->error() != QNetworkReply::NoError ||
            reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() != 200 ||
            body.size() > 2 * 1024 * 1024 || error.error != QJsonParseError::NoError ||
            !json.isArray()) {
            if (manual)
                QMessageBox::warning(window, tr("Jinra updates"),
                                     tr("Could not check for updates. Please try again later."));
            return;
        }
        const auto current = Jinra::VersionNumber::Parse(Jinra::CurrentVersion);
        if (!current)
            return;
        const auto release = Jinra::FindUpdate(json.array(), *current, Jinra::PgoBuild);
        if (release) {
            action->setText(tr("Check for updates... (update available)"));
            if (manual || can_install())
                Offer(*release);
        } else if (manual) {
            QMessageBox::information(window, tr("Jinra updates"),
                                     tr("No newer installable Windows release is available.\n"
                                        "Current version: %1")
                                         .arg(QString::fromLatin1(Jinra::CurrentVersion)));
        }
    });
}

void JinraUpdater::Offer(const Jinra::UpdateRelease& release) {
    if (!can_install()) {
        QMessageBox::information(window, tr("Jinra updates"),
                                 tr("An update is available. Stop the game before installing it."));
        return;
    }
    const auto answer = QMessageBox::question(
        window, tr("Jinra update available"),
        tr("%1 is available.\nCurrent version: %2\n\nDownload the Windows installer?\n"
           "Your profiles and saved data will be kept.")
            .arg(release.name, QString::fromLatin1(Jinra::CurrentVersion)),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer == QMessageBox::Yes)
        Download(release);
}

void JinraUpdater::Download(const Jinra::UpdateRelease& release) {
    if (active || download_progress || !can_install())
        return;
    const QString directory = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) +
                              QStringLiteral("/jinra-updates");
    if (!QDir().mkpath(directory)) {
        QMessageBox::warning(window, tr("Jinra updates"),
                             tr("Could not create the download folder."));
        return;
    }
    auto* progress =
        new QProgressDialog(tr("Downloading Jinra update..."), tr("Cancel"), 0, 1000, window);
    download_progress = progress;
    progress->setWindowModality(Qt::WindowModal);
    progress->setMinimumDuration(0);
    progress->setAutoClose(false);
    progress->setAutoReset(false);
    connect(progress, &QProgressDialog::canceled, this, [this] {
        if (active) {
            active->abort();
        }
    });
    action->setEnabled(false);
    progress->show();
    DownloadAttempt(release, directory + QLatin1Char('/') + release.filename, 1);
}

void JinraUpdater::FinishDownload() {
    download_progress->close();
    download_progress->deleteLater();
    download_progress.clear();
    action->setEnabled(true);
}

void JinraUpdater::DownloadAttempt(const Jinra::UpdateRelease& release, const QString& filename,
                                   int attempt) {
    auto* progress = download_progress.data();
    if (progress->wasCanceled() || !can_install()) {
        FinishDownload();
        return;
    }
    auto state = std::make_shared<DownloadState>(filename);
    if (!state->file.open(QIODevice::WriteOnly)) {
        FinishDownload();
        QMessageBox::warning(window, tr("Jinra updates"), tr("Could not save the installer."));
        return;
    }
    auto request = Request(release.installer);
    request.setRawHeader("Accept", "application/octet-stream");
    auto* reply = network.get(request);
    active = reply;
    connect(reply, &QNetworkReply::downloadProgress, progress,
            [progress, release](qint64 received, qint64) {
                progress->setValue(static_cast<int>(std::clamp<qint64>(received, 0, release.size) *
                                                    1000 / release.size));
            });
    const auto consume = [reply, state, release] {
        const auto chunk = reply->readAll();
        if (chunk.isEmpty() ||
            reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() != 200)
            return;
        if (chunk.size() > release.size - state->bytes ||
            state->file.write(chunk) != chunk.size()) {
            state->valid = false;
            if (reply->isRunning()) {
                reply->abort();
            }
            return;
        }
        state->bytes += chunk.size();
        state->hash.addData(chunk);
    };
    connect(reply, &QNetworkReply::readyRead, this, consume);
    connect(
        reply, &QNetworkReply::finished, this,
        [this, reply, progress, state, release, filename, consume, attempt] {
            consume();
            active = nullptr;
            reply->deleteLater();
            if (progress->wasCanceled()) {
                FinishDownload();
                return;
            }
            if (state->valid && attempt < download_attempts && IsTransientDownloadError(*reply)) {
                state->file.cancelWriting();
                progress->setLabelText(tr("Retrying download (%1 of %2)...")
                                           .arg(attempt + 1)
                                           .arg(download_attempts));
                progress->setValue(0);
                QTimer::singleShot(1000 * attempt, this, [this, release, filename, attempt] {
                    DownloadAttempt(release, filename, attempt + 1);
                });
                return;
            }
            FinishDownload();
            if (!state->valid || reply->error() != QNetworkReply::NoError ||
                reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() != 200 ||
                state->bytes != release.size || state->hash.result() != release.sha256 ||
                !state->file.commit()) {
                QMessageBox::warning(
                    window, tr("Jinra updates"),
                    tr("The update could not be verified. The installer was not started."));
                return;
            }
            if (!can_install())
                return;
            if (QMessageBox::question(
                    window, tr("Install Jinra update"),
                    tr("The download has been verified. Close Jinra and start the installer?"),
                    QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
                return;
#ifdef _WIN32
            if (!QProcess::startDetached(
                    filename, {QStringLiteral("/SILENT"), QStringLiteral("/NORESTART"),
                               QStringLiteral("/CLOSEAPPLICATIONS"),
                               QStringLiteral("/DIR=") + QCoreApplication::applicationDirPath()})) {
                QMessageBox::warning(window, tr("Jinra updates"),
                                     tr("Could not start the installer."));
                return;
            }
            exit_for_install();
#else
            QMessageBox::information(window, tr("Jinra updates"),
                                     tr("The installer is for Windows only."));
#endif
        });
}
