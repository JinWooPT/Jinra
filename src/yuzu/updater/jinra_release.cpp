// SPDX-License-Identifier: GPL-3.0-or-later
#include <QJsonObject>
#include <QRegularExpression>
#include "yuzu/updater/jinra_release.h"

namespace Jinra {

std::optional<UpdateRelease> FindUpdate(const QJsonArray& releases, const VersionNumber& current,
                                       bool pgo_build) {
    std::optional<UpdateRelease> best;
    const QRegularExpression digest_pattern(QStringLiteral("^sha256:([0-9a-fA-F]{64})$"));
    for (const auto& entry : releases) {
        const auto object = entry.toObject();
        if (object.value(QStringLiteral("draft")).toBool())
            continue;
        const auto tag = object.value(QStringLiteral("tag_name")).toString();
        const auto version = VersionNumber::Parse(tag.toStdString());
        if (!version || *version <= current || (best && *version <= best->version))
            continue;
        const auto plain_version = tag.startsWith(QLatin1Char('v')) ? tag.mid(1) : tag;
        const auto filename = QStringLiteral("Jinra-Setup-%1-x64%2.exe")
                                  .arg(plain_version, pgo_build ? QStringLiteral("-pgo") : QString{});
        const QUrl expected_page(QStringLiteral("https://github.com/JinWooPT/Jinra/releases/tag/") +
                                 tag);
        if (QUrl(object.value(QStringLiteral("html_url")).toString()) != expected_page)
            continue;
        for (const auto& item : object.value(QStringLiteral("assets")).toArray()) {
            const auto asset = item.toObject();
            if (asset.value(QStringLiteral("name")).toString() != filename ||
                asset.value(QStringLiteral("state")).toString() != QStringLiteral("uploaded"))
                continue;
            const QUrl url(asset.value(QStringLiteral("browser_download_url")).toString());
            const QUrl expected(
                QStringLiteral("https://github.com/JinWooPT/Jinra/releases/download/") + tag +
                QLatin1Char('/') + filename);
            const auto match =
                digest_pattern.match(asset.value(QStringLiteral("digest")).toString());
            const auto size = asset.value(QStringLiteral("size")).toInteger();
            if (url != expected || !match.hasMatch() || size <= 0 || size > 512LL * 1024 * 1024)
                continue;
            best = UpdateRelease{tag,           object.value(QStringLiteral("name")).toString(tag),
                                 expected_page, url,
                                 filename,      QByteArray::fromHex(match.captured(1).toLatin1()),
                                 size,          *version};
            break;
        }
    }
    return best;
}

} // namespace Jinra
