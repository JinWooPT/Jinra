// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <optional>
#include <QByteArray>
#include <QJsonArray>
#include <QString>
#include <QUrl>
#include "common/jinra_version.h"

namespace Jinra {

struct UpdateRelease {
    QString tag;
    QString name;
    QUrl page;
    QUrl installer;
    QString filename;
    QByteArray sha256;
    qint64 size{};
    VersionNumber version;
};

std::optional<UpdateRelease> FindUpdate(const QJsonArray& releases, const VersionNumber& current,
                                       bool pgo_build = false);

} // namespace Jinra
