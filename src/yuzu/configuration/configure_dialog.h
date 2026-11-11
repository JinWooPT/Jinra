// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2016 Citra Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <memory>
#include <optional>
#include <utility>
#include <vector>
#include <QDialog>
#include <QList>
#include "configuration/shared_widget.h"
#include "qt_common/config/shared_translation.h"
#include "qt_common/util/vk.h"
#include "yuzu/configuration/configuration_shared.h"

namespace Core {
class System;
}

class ConfigureApplets;
class ConfigureAudio;
class ConfigureCpu;
class ConfigureDebugTab;
class ConfigureFilesystem;
class ConfigureGeneral;
class ConfigureGraphics;
class ConfigureGraphicsAdvanced;
class ConfigureGraphicsExtensions;
class ConfigureHotkeys;
class ConfigureInput;
class ConfigureProfileManager;
class ConfigureSystem;
class ConfigureNetwork;
class ConfigureUi;
class ConfigureWeb;

class HotkeyRegistry;

namespace InputCommon {
class InputSubsystem;
}

namespace Ui {
class ConfigureDialog;
}

class ConfigurationPages final : public QObject {
    Q_OBJECT

public:
    static constexpr int CategoryCount = 6;

    ConfigurationPages(QWidget* parent, HotkeyRegistry& registry_,
                       InputCommon::InputSubsystem* input_subsystem,
                       std::vector<VkDeviceInfo::Record>& vk_device_records,
                       Core::System& system_, bool enable_web_config);
    ~ConfigurationPages() override;
    void ApplyConfiguration();
    void UseHomeLayout() { home_layout = true; }
    static std::array<QString, CategoryCount> CategoryTitles();
    QList<QWidget*> LoadCategory(int category);
    void ResetDebugTab();

signals:
    void LanguageChanged(const QString& locale);
    void ExternalContentDirsChanged();
    void ResetRequested();

private:
    HotkeyRegistry& registry;
    Core::System& system;
    InputCommon::InputSubsystem* input_subsystem;
    std::vector<VkDeviceInfo::Record>& vk_device_records;
    const bool enable_web_config;
    bool home_layout{};
    std::unique_ptr<QWidget> staging;
    std::unique_ptr<ConfigurationShared::Builder> builder;
    std::optional<std::pair<Settings::AspectRatio, Settings::ResolutionSetup>> screenshot_info;

    std::unique_ptr<ConfigureApplets> applets_tab;
    std::unique_ptr<ConfigureAudio> audio_tab;
    std::unique_ptr<ConfigureCpu> cpu_tab;
    std::unique_ptr<ConfigureDebugTab> debug_tab_tab;
    std::unique_ptr<ConfigureFilesystem> filesystem_tab;
    std::unique_ptr<ConfigureGeneral> general_tab;
    std::unique_ptr<ConfigureGraphicsAdvanced> graphics_advanced_tab;
    std::unique_ptr<ConfigureGraphicsExtensions> graphics_extensions_tab;
    std::unique_ptr<ConfigureUi> ui_tab;
    std::unique_ptr<ConfigureGraphics> graphics_tab;
    std::unique_ptr<ConfigureHotkeys> hotkeys_tab;
    std::unique_ptr<ConfigureInput> input_tab;
    std::unique_ptr<ConfigureNetwork> network_tab;
    std::unique_ptr<ConfigureProfileManager> profile_tab;
    std::unique_ptr<ConfigureSystem> system_tab;
    std::unique_ptr<ConfigureWeb> web_tab;
};

class ConfigureDialog : public QDialog {
    Q_OBJECT

public:
    explicit ConfigureDialog(QWidget* parent, HotkeyRegistry& registry,
                             InputCommon::InputSubsystem* input_subsystem,
                             std::vector<VkDeviceInfo::Record>& vk_device_records,
                             Core::System& system, bool enable_web_config = true);
    ~ConfigureDialog() override;

    void ApplyConfiguration();
    void SelectCategory(int index);

signals:
    void LanguageChanged(const QString& locale);
    void ExternalContentDirsChanged();

private:
    void changeEvent(QEvent* event) override;
    void RetranslateUI();
    void HandleApplyButtonClicked();
    void UpdateVisibleTabs();
    void PopulateSelectionList();

    std::unique_ptr<Ui::ConfigureDialog> ui;
    ConfigurationPages settings;
};
