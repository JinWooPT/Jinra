// SPDX-License-Identifier: GPL-3.0-or-later
#include <QJsonArray>
#include <QJsonObject>
#include <catch2/catch_test_macros.hpp>
#include "common/jinra_version.h"
#include "yuzu/updater/jinra_release.h"

namespace {
QJsonObject Release(QString tag) {
    const auto version = tag.startsWith(QLatin1Char('v')) ? tag.mid(1) : tag;
    const auto name = QStringLiteral("Jinra-Setup-%1-x64.exe").arg(version);
    return {{"tag_name", tag},
            {"name", tag},
            {"draft", false},
            {"html_url", QStringLiteral("https://github.com/JinWooPT/Jinra/releases/tag/") + tag},
            {"assets", QJsonArray{QJsonObject{
                           {"name", name},
                           {"state", "uploaded"},
                           {"size", 30000000},
                           {"digest", QStringLiteral("sha256:") + QString(64, QLatin1Char('a'))},
                           {"browser_download_url",
                            QStringLiteral("https://github.com/JinWooPT/Jinra/releases/download/") +
                                tag + QLatin1Char('/') + name}}}}};
}
} // namespace

TEST_CASE("Jinra versions order previews, stable releases and numeric components",
          "[jinra-update]") {
    using V = Jinra::VersionNumber;
    REQUIRE(V::Parse("v0.1.0-preview.1"));
    REQUIRE(V::Parse("0.1.0-preview.0"));
    REQUIRE(*V::Parse("0.1.0-preview.2") > *V::Parse("0.1.0-preview.1"));
    REQUIRE(*V::Parse("0.1.0") > *V::Parse("0.1.0-preview.99"));
    REQUIRE(*V::Parse("0.10.0") > *V::Parse("0.9.99"));
    REQUIRE(*V::Parse("v1.0.0") == *V::Parse("1.0.0"));
    for (const auto text : {"", "1.2", "1.2.3.4", "01.2.3", "1.2.3-beta.1", "1.2.3-preview.",
                            "1.2.3-preview.01", "4294967296.0.0", "1.-2.0", "1.2.3junk"})
        REQUIRE_FALSE(V::Parse(text));
}

TEST_CASE("Jinra only selects newer releases with a trusted installer and digest",
          "[jinra-update]") {
    const auto current = *Jinra::VersionNumber::Parse("0.1.0-preview.1");
    const auto valid = Release(QStringLiteral("v0.1.0-preview.2"));
    const auto result = Jinra::FindUpdate({valid}, current);
    REQUIRE(result);
    REQUIRE(result->sha256.size() == 32);
    REQUIRE(result->size == 30000000);
    REQUIRE_FALSE(Jinra::FindUpdate({Release(QStringLiteral("v0.1.0-preview.1"))}, current));
    REQUIRE_FALSE(Jinra::FindUpdate({Release(QStringLiteral("v0.0.9"))}, current));
    auto draft = valid;
    draft["draft"] = true;
    REQUIRE_FALSE(Jinra::FindUpdate({draft}, current));
    auto foreign = valid;
    foreign["html_url"] = "https://github.com/someone/other/releases/tag/v0.1.0-preview.2";
    REQUIRE_FALSE(Jinra::FindUpdate({foreign}, current));
    for (const auto field : {"name", "state", "digest", "browser_download_url", "size"}) {
        auto malformed = valid;
        auto asset = malformed["assets"].toArray().at(0).toObject();
        asset.remove(field);
        malformed["assets"] = QJsonArray{asset};
        REQUIRE_FALSE(Jinra::FindUpdate({malformed}, current));
    }
    for (const auto bad_url : {"http://github.com/JinWooPT/Jinra/releases/download/"
                               "v0.1.0-preview.2/Jinra-Setup-0.1.0-preview.2-x64.exe",
                               "https://example.com/installer.exe", "file:///C:/installer.exe"}) {
        auto malformed = valid;
        auto asset = malformed["assets"].toArray().at(0).toObject();
        asset["browser_download_url"] = bad_url;
        malformed["assets"] = QJsonArray{asset};
        REQUIRE_FALSE(Jinra::FindUpdate({malformed}, current));
    }
    const auto latest =
        Jinra::FindUpdate({valid, Release("v0.1.0"), Release("v0.1.0-preview.8")}, current);
    REQUIRE(latest);
    REQUIRE(latest->tag == QStringLiteral("v0.1.0"));
}
