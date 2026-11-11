// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QDialog>
#include <QPixmap>
#include <QString>
#include <QVariant>
#include <QVector>

class QComboBox;
class QLabel;
class QListWidget;
class QLineEdit;
class QNetworkAccessManager;
class QPushButton;

class JinraAvatarDialog final : public QDialog {
    Q_OBJECT
public:
    struct Profile {
        QString name;
        QPixmap image;
    };

    explicit JinraAvatarDialog(QWidget* parent);
    static Profile ActiveProfile();

private:
    struct Avatar {
        QString path;
        QString name;
    };

    void SaveProfiles();
    void UpdateProfile();
    void RenameProfile(bool create);
    void SetImage(const QPixmap& image);
    void RefreshCollections();
    void LoadCollection();
    void RenderPage();
    void SelectAvatar();
    void ToggleFavorite();

    QVariantList profiles;
    QVariantMap favorites;
    QComboBox* profile_box{};
    QComboBox* collection_box{};
    QLineEdit* search{};
    QListWidget* cards{};
    QLabel* avatar_image{};
    QLabel* status{};
    QLabel* page_count{};
    QPushButton* favorite_filter{};
    QPushButton* previous{};
    QPushButton* next{};
    QPushButton* favorite{};
    QPushButton* use{};
    QNetworkAccessManager* network{};
    QVector<Avatar> collection;
    QVector<Avatar> page_items;
    int page{};
    int collection_request{};
    int page_request{};
};
