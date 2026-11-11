// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <array>
#include <functional>
#include <memory>
#include <vector>
#include <QPersistentModelIndex>
#include <QWidget>
#include "common/param_package.h"
#include "yuzu/jinra_home_audio.h"

class GameList;
class QAbstractButton;
class QCheckBox;
class QComboBox;
class QHBoxLayout;
class QLabel;
class QPushButton;
class QStackedWidget;
class QTimer;
class QToolButton;
class JinraControllerView;
class JinraHomeSettings;
class ConfigurationPages;
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
    bool IsHomeChromeActive() const;
    void ShowSettings(std::unique_ptr<ConfigurationPages> settings, int category,
                      std::function<void(bool)> finished);

signals:
    void AddDirectory();
    void InstallContentRequested();
    void HomeChromeChanged(bool active);
    void ConfigureRequested(int category);
    void BootRequested(const QString& path);
    void PropertiesRequested(const QString& path);
    void SaveSettings();
    void LanguageRequested(const QString& locale);

protected:
    void changeEvent(QEvent* event) override;

private:
    QWidget* CreateLibrary(QWidget* empty_library);
    void CreateHomeChrome();
    void RefreshHomeProfile();
    void ConnectHomeSound(QAbstractButton* button, JinraHomeAudio::Cue cue);
    void SetHomeLayout(bool enabled);
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
    JinraHomeSettings* home_settings{};
    QWidget* sidebar{};
    QWidget* desktop_header{};
    QWidget* home_header{};
    QWidget* home_footer{};
    QWidget* library_detail{};
    QWidget* library_search{};
    QWidget* home_search{};
    QToolButton* home_search_button{};
    QToolButton* home_sort_button{};
    QHBoxLayout* desktop_search_layout{};
    QHBoxLayout* home_search_layout{};
    QHBoxLayout* collection_row{};
    QWidget* collection_tabs{};
    QLabel* home_clock{};
    QLabel* home_controllers{};
    QLabel* home_count{};
    QPushButton* home_play{};
    QPushButton* home_options{};
    QToolButton* home_save_data{};
    QToolButton* home_game_settings{};
    QToolButton* home_profile{};
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
    JinraHomeAudio home_audio;
    bool home_layout{true};
    bool emulating{};
};
