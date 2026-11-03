// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <array>
#include <vector>
#include <QPersistentModelIndex>
#include <QWidget>
#include "common/param_package.h"

class GameList;
class QCheckBox;
class QComboBox;
class QLabel;
class QPushButton;
class QStackedWidget;
class QTimer;
class JinraControllerView;
namespace InputCommon {
class InputSubsystem;
}

class JinraHome final : public QWidget {
    Q_OBJECT
public:
    JinraHome(GameList* games, QWidget* empty_library, InputCommon::InputSubsystem* input,
              QWidget* parent = nullptr);
    void ReloadSettings();
    void SetEmulating(bool running);

signals:
    void AddDirectory();
    void ConfigureRequested(int category);
    void BootRequested(const QString& path);
    void PropertiesRequested(const QString& path);
    void SaveSettings();
    void LanguageRequested(const QString& locale);

protected:
    void changeEvent(QEvent* event) override;

private:
    QWidget* CreateLibrary(QWidget* empty_library);
    QWidget* CreateGraphics();
    QWidget* CreateControls();
    QWidget* CreateSystem();
    void Retranslate();
    void ApplySettings();
    void RefreshControllers();
    void SelectGame(const QModelIndex& index);
    void SelectFirstGame();

    GameList* games;
    InputCommon::InputSubsystem* input;
    QStackedWidget* pages{};
    std::array<QPushButton*, 4> navigation{};
    std::array<QLabel*, 4> headings{};
    QLabel* detail_image{};
    QLabel* detail_title{};
    QLabel* detail_status{};
    QPushButton* play{};
    QPushButton* properties{};
    QLabel* library_status{};
    QPersistentModelIndex selected_game;
    QComboBox* renderer{};
    QComboBox* resolution{};
    QComboBox* antialiasing{};
    QComboBox* filter{};
    QComboBox* accuracy{};
    QCheckBox* async_shaders{};
    QComboBox* memory{};
    QComboBox* console_mode{};
    QComboBox* language{};
    QComboBox* player{};
    QComboBox* controller_layout{};
    JinraControllerView* controller_view{};
    QLabel* controller_status{};
    QLabel* mapping_status{};
    QLabel* mapping_heading{};
    std::array<QLabel*, 8> mappings{};
    QTimer* controller_timer{};
    bool emulating{};
};
