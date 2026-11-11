// SPDX-License-Identifier: GPL-3.0-or-later
#include "yuzu/jinra_avatar.h"

#include <algorithm>
#include <array>
#include <QBuffer>
#include <QComboBox>
#include <QFileDialog>
#include <QGridLayout>
#include <QImageReader>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPushButton>
#include <QRegularExpression>
#include <QSettings>
#include <QSignalBlocker>

#include "common/fs/path_util.h"

namespace {
constexpr int PageSize = 12;
constexpr auto CollectionsUrl = "https://api.github.com/repos/henry-debruin/nso-icons/contents";
constexpr auto ArtworkUrl = "https://raw.githubusercontent.com/henry-debruin/nso-icons/main/";
constexpr auto HelloKitty = "hello-kitty-island-adventure";
// Hello Kitty Island Adventure character artwork by SANRIO and Sunblink.
constexpr std::array<std::pair<const char*, const char*>, 13> HelloKittyAvatars{{
    {"kitty", "Hello Kitty"}, {"cinna", "Cinnamoroll"}, {"kuromi", "Kuromi"},
    {"mel", "My Melody"}, {"hangy", "Hangyodon"}, {"retsuko", "Retsuko"},
    {"badtz", "Badtz-maru"}, {"choco", "Chococat"}, {"kero", "Keroppi"},
    {"pom", "Pompompurin"}, {"poch", "Pochacco"}, {"peck", "Pekkle"}, {"tux", "Tuxedosam"},
}};

QString ProfileFile() {
    const auto path = Common::FS::GetEdenPath(Common::FS::EdenPath::ConfigDir) / "home-profiles.ini";
    return QString::fromStdString(Common::FS::PathToUTF8String(path));
}

QVariantList ReadProfiles(const QSettings& settings) {
    auto profiles = settings.value(QStringLiteral("profiles")).toList();
    if (profiles.isEmpty()) {
        profiles.append(QVariantMap{{QStringLiteral("name"), QObject::tr("Player")}});
    }
    return profiles;
}

QPixmap ProfileImage(const QVariantMap& profile) {
    QPixmap image;
    image.loadFromData(profile.value(QStringLiteral("image")).toByteArray(), "PNG");
    if (image.isNull()) {
        image.load(QStringLiteral(":/jinra/icon.png"));
    }
    return image;
}

QNetworkRequest AvatarRequest(const QString& url) {
    QNetworkRequest request{QUrl(url)};
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Jinra"));
    request.setTransferTimeout(12000);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    return request;
}

QString DisplayName(QString slug) {
    auto words = slug.replace(QLatin1Char('-'), QLatin1Char(' ')).split(QLatin1Char(' '), Qt::SkipEmptyParts);
    for (auto& word : words) {
        word[0] = word[0].toUpper();
    }
    return words.join(QLatin1Char(' '));
}
} // namespace

JinraAvatarDialog::Profile JinraAvatarDialog::ActiveProfile() {
    QSettings settings(ProfileFile(), QSettings::IniFormat);
    const auto profiles = ReadProfiles(settings);
    const auto index = std::clamp<qsizetype>(settings.value(QStringLiteral("active"), 0).toInt(),
                                            0, profiles.size() - 1);
    const auto profile = profiles[index].toMap();
    return {profile.value(QStringLiteral("name")).toString(), ProfileImage(profile)};
}

JinraAvatarDialog::JinraAvatarDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle(tr("Profile · Jinra"));
    setObjectName(QStringLiteral("JinraAvatarDialog"));
    setMinimumSize(620, 480);
    resize(900, 620);
    QSettings settings(ProfileFile(), QSettings::IniFormat);
    profiles = ReadProfiles(settings);
    favorites = settings.value(QStringLiteral("favorites")).toMap();
    network = new QNetworkAccessManager(this);

    auto* body = new QVBoxLayout(this);
    body->setContentsMargins(24, 22, 24, 22);
    body->setSpacing(15);
    auto* header = new QHBoxLayout;
    avatar_image = new QLabel;
    avatar_image->setFixedSize(90, 90);
    avatar_image->setAlignment(Qt::AlignCenter);
    avatar_image->setObjectName(QStringLiteral("JinraAvatarPreview"));
    header->addWidget(avatar_image);
    auto* heading = new QVBoxLayout;
    auto* title = new QLabel(tr("Profile"));
    title->setObjectName(QStringLiteral("JinraHeading"));
    heading->addWidget(title);
    auto* description = new QLabel(tr("Your name and avatar appear in Jinra Home. Game saves use your console profile."));
    description->setObjectName(QStringLiteral("JinraMuted"));
    description->setWordWrap(true);
    heading->addWidget(description);
    header->addLayout(heading, 1);
    profile_box = new QComboBox;
    profile_box->setAccessibleName(tr("Home profile"));
    profile_box->setMinimumWidth(145);
    for (const auto& profile : std::as_const(profiles)) {
        profile_box->addItem(profile.toMap().value(QStringLiteral("name")).toString());
    }
    profile_box->setCurrentIndex(std::clamp(settings.value(QStringLiteral("active"), 0).toInt(),
                                            0, profile_box->count() - 1));
    header->addWidget(profile_box);
    body->addLayout(header);

    auto* actions = new QHBoxLayout;
    auto* rename = new QPushButton(tr("Rename"));
    auto* add = new QPushButton(tr("Add profile"));
    auto* import = new QPushButton(tr("Import image"));
    auto* remove = new QPushButton(tr("Remove avatar"));
    for (auto* button : {rename, add, import, remove}) {
        button->setObjectName(QStringLiteral("JinraAvatarAction"));
        actions->addWidget(button);
    }
    actions->addStretch();
    body->addLayout(actions);
    connect(rename, &QPushButton::clicked, this, [this] { RenameProfile(false); });
    connect(add, &QPushButton::clicked, this, [this] { RenameProfile(true); });
    connect(remove, &QPushButton::clicked, this, [this] { SetImage({}); });
    connect(import, &QPushButton::clicked, this, [this] {
        const auto path = QFileDialog::getOpenFileName(this, tr("Import avatar"), {},
                                                       tr("Images (*.png *.jpg *.jpeg *.webp)"));
        if (path.isEmpty()) {
            return;
        }
        QImageReader reader(path);
        const auto size = reader.size();
        if (size.isValid()) {
            reader.setScaledSize(size.scaled(256, 256, Qt::KeepAspectRatio));
        }
        const auto image = QPixmap::fromImage(reader.read());
        if (!image.isNull()) {
            SetImage(image);
        } else {
            status->setText(tr("This image could not be opened."));
        }
    });

    auto* filters = new QHBoxLayout;
    collection_box = new QComboBox;
    collection_box->setAccessibleName(tr("Avatar collection"));
    collection_box->addItem(tr("Hello Kitty & Friends"), QLatin1String(HelloKitty));
    collection_box->addItem(tr("Ocarina of Time"), QStringLiteral("the-legend-of-zelda-ocarina-of-time"));
    collection_box->addItem(tr("Pokémon Pokopia"), QStringLiteral("pokemon-pokopia"));
    collection_box->addItem(tr("Pokémon Legends: Z-A"), QStringLiteral("pokemon-legends-za"));
    collection_box->addItem(tr("Pokémon Scarlet and Violet"), QStringLiteral("pokemon-scarlet-and-pokemon-violet"));
    auto* more = new QPushButton(tr("More collections"));
    favorite_filter = new QPushButton(tr("Favorites"));
    favorite_filter->setCheckable(true);
    search = new QLineEdit;
    search->setPlaceholderText(tr("Search avatars"));
    search->setClearButtonEnabled(true);
    filters->addWidget(collection_box, 1);
    filters->addWidget(favorite_filter);
    filters->addWidget(search, 1);
    filters->addWidget(more);
    body->addLayout(filters);
    connect(more, &QPushButton::clicked, this, &JinraAvatarDialog::RefreshCollections);

    cards = new QListWidget;
    cards->setObjectName(QStringLiteral("JinraAvatarCards"));
    cards->setViewMode(QListView::IconMode);
    cards->setMovement(QListView::Static);
    cards->setResizeMode(QListView::Adjust);
    cards->setIconSize(QSize(100, 100));
    cards->setGridSize(QSize(132, 133));
    cards->setWordWrap(true);
    body->addWidget(cards, 1);

    auto* pagination = new QHBoxLayout;
    previous = new QPushButton(tr("Previous"));
    next = new QPushButton(tr("Next"));
    page_count = new QLabel;
    favorite = new QPushButton(tr("Add to favorites"));
    pagination->addWidget(previous);
    pagination->addWidget(page_count);
    pagination->addWidget(next);
    pagination->addStretch();
    pagination->addWidget(favorite);
    body->addLayout(pagination);

    auto* footer = new QHBoxLayout;
    status = new QLabel;
    status->setObjectName(QStringLiteral("JinraMuted"));
    status->setWordWrap(true);
    footer->addWidget(status, 1);
    auto* close = new QPushButton(tr("Close"));
    use = new QPushButton(tr("Use avatar"));
    use->setObjectName(QStringLiteral("JinraPrimary"));
    footer->addWidget(close);
    footer->addWidget(use);
    body->addLayout(footer);

    connect(profile_box, &QComboBox::currentIndexChanged, this, [this] {
        SaveProfiles();
        UpdateProfile();
    });
    connect(collection_box, &QComboBox::currentIndexChanged, this, &JinraAvatarDialog::LoadCollection);
    connect(favorite_filter, &QPushButton::toggled, this, [this](bool enabled) {
        collection_box->setEnabled(!enabled);
        LoadCollection();
    });
    connect(search, &QLineEdit::textChanged, this, [this] {
        page = 0;
        RenderPage();
    });
    connect(previous, &QPushButton::clicked, this, [this] { --page; RenderPage(); });
    connect(next, &QPushButton::clicked, this, [this] { ++page; RenderPage(); });
    connect(cards, &QListWidget::currentItemChanged, this, &JinraAvatarDialog::SelectAvatar);
    connect(favorite, &QPushButton::clicked, this, &JinraAvatarDialog::ToggleFavorite);
    connect(close, &QPushButton::clicked, this, &QDialog::accept);
    connect(use, &QPushButton::clicked, this, [this] {
        if (const auto* item = cards->currentItem(); item && !item->icon().isNull()) {
            SetImage(item->data(Qt::UserRole).value<QPixmap>());
        }
    });
    UpdateProfile();
    LoadCollection();
}

void JinraAvatarDialog::SaveProfiles() {
    QSettings settings(ProfileFile(), QSettings::IniFormat);
    settings.setValue(QStringLiteral("profiles"), profiles);
    settings.setValue(QStringLiteral("active"), profile_box->currentIndex());
    settings.setValue(QStringLiteral("favorites"), favorites);
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        status->setText(tr("The profile settings could not be saved."));
    }
}

void JinraAvatarDialog::UpdateProfile() {
    avatar_image->setPixmap(ProfileImage(profiles[profile_box->currentIndex()].toMap())
                                .scaled(78, 78, Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

void JinraAvatarDialog::RenameProfile(bool create) {
    bool accepted{};
    const auto name = QInputDialog::getText(this, create ? tr("Add profile") : tr("Rename profile"),
                                            tr("Display name"), QLineEdit::Normal,
                                            create ? QString{} : profile_box->currentText(),
                                            &accepted).trimmed().left(40);
    if (!accepted || name.isEmpty()) {
        return;
    }
    if (create) {
        profiles.append(QVariantMap{{QStringLiteral("name"), name}});
        profile_box->addItem(name);
        profile_box->setCurrentIndex(profile_box->count() - 1);
    } else {
        const int index = profile_box->currentIndex();
        auto profile = profiles[index].toMap();
        profile.insert(QStringLiteral("name"), name);
        profiles[index] = profile;
        profile_box->setItemText(index, name);
        SaveProfiles();
    }
}

void JinraAvatarDialog::SetImage(const QPixmap& image) {
    QByteArray png;
    if (!image.isNull()) {
        QBuffer buffer(&png);
        buffer.open(QIODevice::WriteOnly);
        if (!image.scaled(256, 256, Qt::KeepAspectRatio, Qt::SmoothTransformation).save(&buffer, "PNG")) {
            status->setText(tr("The avatar could not be saved."));
            return;
        }
    }
    const int index = profile_box->currentIndex();
    auto profile = profiles[index].toMap();
    profile.insert(QStringLiteral("image"), png);
    profiles[index] = profile;
    UpdateProfile();
    SaveProfiles();
}

void JinraAvatarDialog::RefreshCollections() {
    auto* reply = network->get(AvatarRequest(QLatin1String(CollectionsUrl) + QStringLiteral("?ref=main")));
    status->setText(tr("Loading collections…"));
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        const auto entries = QJsonDocument::fromJson(reply->readAll()).array();
        if (reply->error() != QNetworkReply::NoError || entries.isEmpty()) {
            status->setText(tr("Collections are unavailable. Try again later."));
            return;
        }
        static const QRegularExpression slug(QStringLiteral("^[a-z0-9][a-z0-9-]{0,100}$"));
        for (const auto& entry : entries) {
            const auto item = entry.toObject();
            const auto path = item.value(QStringLiteral("path")).toString();
            if (item.value(QStringLiteral("type")).toString() == QStringLiteral("dir") &&
                slug.match(path).hasMatch() && collection_box->findData(path) < 0) {
                collection_box->addItem(DisplayName(path), path);
            }
        }
        status->setText(tr("Choose a collection to browse its avatars."));
    });
}

void JinraAvatarDialog::LoadCollection() {
    const int request = ++collection_request;
    collection.clear();
    page = 0;
    if (favorite_filter->isChecked()) {
        for (auto entry = favorites.cbegin(); entry != favorites.cend(); ++entry) {
            collection.append({entry.key(), entry.value().toString()});
        }
    } else if (collection_box->currentData().toString() == QLatin1String(HelloKitty)) {
        for (const auto& [path, name] : HelloKittyAvatars) {
            collection.append({QStringLiteral(":/jinra/avatars/%1.png").arg(QLatin1String(path)),
                               QLatin1String(name)});
        }
    } else {
        const auto folder = collection_box->currentData().toString() + QStringLiteral("/characters");
        auto* reply = network->get(AvatarRequest(QLatin1String(CollectionsUrl) + QLatin1Char('/') +
                                                  folder + QStringLiteral("?ref=main")));
        connect(reply, &QNetworkReply::finished, this, [this, reply, folder, request] {
            reply->deleteLater();
            if (request != collection_request) {
                return;
            }
            static const QRegularExpression file(QStringLiteral("^[A-Za-z0-9_ .-]{1,100}\\.png$"));
            const auto entries = QJsonDocument::fromJson(reply->readAll()).array();
            for (const auto& entry : entries) {
                const auto name = entry.toObject().value(QStringLiteral("name")).toString();
                if (file.match(name).hasMatch()) {
                    collection.append({folder + QLatin1Char('/') + name, DisplayName(name.chopped(4))});
                }
            }
            RenderPage();
            if (collection.isEmpty()) {
                status->setText(tr("This collection could not be loaded."));
            }
        });
        RenderPage();
        status->setText(tr("Loading avatars…"));
        return;
    }
    RenderPage();
}

void JinraAvatarDialog::RenderPage() {
    const int request = ++page_request;
    cards->clear();
    page_items.clear();
    const auto query = search->text().trimmed();
    QVector<Avatar> filtered;
    for (const auto& avatar : std::as_const(collection)) {
        if (avatar.name.contains(query, Qt::CaseInsensitive)) {
            filtered.append(avatar);
        }
    }
    const int count = static_cast<int>(filtered.size());
    page = std::clamp(page, 0, std::max(0, (count - 1) / PageSize));
    const int start = page * PageSize;
    const int end = std::min(start + PageSize, count);
    previous->setEnabled(page > 0);
    next->setEnabled(end < count);
    page_count->setText(count > 0 ? tr("%1–%2 of %3").arg(start + 1).arg(end).arg(count) : tr("0 avatars"));
    for (int index = start; index < end; ++index) {
        const auto& avatar = filtered[index];
        const int row = static_cast<int>(page_items.size());
        page_items.append(avatar);
        auto* item = new QListWidgetItem(avatar.name, cards);
        const auto set_image = [](QListWidgetItem* target, const QPixmap& image) {
            if (!image.isNull()) {
                target->setIcon(QIcon(image));
                target->setData(Qt::UserRole, image);
            }
        };
        if (avatar.path.startsWith(QLatin1String(":/jinra/avatars/"))) {
            set_image(item, QPixmap(avatar.path));
            continue;
        }
        static const QRegularExpression path(QStringLiteral("^[a-z0-9-]+/characters/[A-Za-z0-9_ .-]+\\.png$"));
        if (!path.match(avatar.path).hasMatch()) {
            continue;
        }
        auto* reply = network->get(AvatarRequest(QLatin1String(ArtworkUrl) + avatar.path));
        connect(reply, &QNetworkReply::finished, this, [this, reply, request, row, set_image] {
            reply->deleteLater();
            if (request != page_request || reply->error() != QNetworkReply::NoError ||
                reply->bytesAvailable() > 4 * 1024 * 1024) {
                return;
            }
            QPixmap image;
            image.loadFromData(reply->readAll(), "PNG");
            set_image(cards->item(row), image);
            if (cards->currentRow() == row) {
                SelectAvatar();
            }
        });
    }
    SelectAvatar();
}

void JinraAvatarDialog::SelectAvatar() {
    const int row = cards->currentRow();
    const bool selected = row >= 0 && row < page_items.size();
    const bool loaded = selected && !cards->item(row)->icon().isNull();
    favorite->setEnabled(selected);
    use->setEnabled(loaded);
    favorite->setText(selected && favorites.contains(page_items[row].path)
                          ? tr("Remove from favorites") : tr("Add to favorites"));
    if (loaded) {
        const auto image = cards->item(row)->data(Qt::UserRole).value<QPixmap>();
        avatar_image->setPixmap(image.scaled(78, 78, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        status->setText(tr("Previewing %1").arg(page_items[row].name));
    } else {
        UpdateProfile();
        status->setText(selected ? tr("Waiting for the avatar image…")
                                 : tr("Choose an avatar to preview it."));
    }
}

void JinraAvatarDialog::ToggleFavorite() {
    const int row = cards->currentRow();
    if (row < 0 || row >= page_items.size()) {
        return;
    }
    const auto& avatar = page_items[row];
    if (!favorites.remove(avatar.path)) {
        favorites.insert(avatar.path, avatar.name);
    }
    SaveProfiles();
    if (favorite_filter->isChecked()) {
        LoadCollection();
    } else {
        SelectAvatar();
    }
}
