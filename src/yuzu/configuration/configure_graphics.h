// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2016 Citra Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <type_traits>
#include <typeindex>
#include <vector>
#include <QColor>
#include <QString>
#include <QWidget>
#include <qobjectdefs.h>
#include <vulkan/vulkan_core.h>
#include "common/common_types.h"
#include "common/settings_enums.h"
#include "qt_common/config/shared_translation.h"
#include "qt_common/util/vk.h"
#include "yuzu/configuration/configuration_shared.h"

class QPushButton;
class QEvent;
class QObject;
class QComboBox;
class QCheckBox;
class QLabel;
class QVBoxLayout;
class AmdFsr4Panel;

namespace Settings {
enum class NvdecEmulation : u32;
enum class RendererBackend : u32;
} // namespace Settings

namespace Core {
class System;
}

namespace Ui {
class ConfigureGraphics;
}

namespace ConfigurationShared {
class Builder;
}

class ConfigureGraphics : public ConfigurationShared::Tab {
    Q_OBJECT

public:
    explicit ConfigureGraphics(
        const Core::System& system_, std::vector<VkDeviceInfo::Record>& records,
        const std::function<void()>& expose_compute_option,
        const std::function<void(Settings::AspectRatio, Settings::ResolutionSetup)>&
            update_aspect_ratio,
        std::shared_ptr<std::vector<ConfigurationShared::Tab*>> group,
        const ConfigurationShared::Builder& builder, QWidget* parent = nullptr);
    ~ConfigureGraphics() override;

    void ApplyConfiguration() override;
    void SetConfiguration() override;

private:
    void changeEvent(QEvent* event) override;
    void RetranslateUI();

    void Setup(const ConfigurationShared::Builder& builder);

    void PopulateVSyncModeSelection(bool use_setting);
    void UpdateVsyncSetting() const;
    void UpdateBackgroundColorButton(QColor color);
    void UpdateAPILayout();
    void UpdateDeviceSelection(int device);
    void UpdateAmdFsr4Visibility();
    void UpdateDlssVisibility();
    /// The selected GPU can run NVIDIA DLSS (Vulkan on an NVIDIA card).
    [[nodiscard]] bool DlssDevice() const;
    /// DLSS would upscale: its checkbox on (per game: the global value) on a DLSS device. A saved
    /// "on" from another PC or build does nothing on AMD or Intel, so it shows nothing there.
    [[nodiscard]] bool DlssOn() const;
    /// Fritchii builds: OoT's real sizes in the DLSS quality names, and the resolution's tooltip
    /// while DLSS or FSR sets OoT's resolution.
    void SetupOcarinaUpscalerLabels();
    void UpdateOcarinaUpscalerLabels();
    /// Fritchii builds: short names and plain tooltips for this page's rows, the resolution
    /// scales as heights ("1080p (720p handheld)"). `rows`: every built row by setting id.
    void SetupPlainNames(const std::map<u32, QWidget*>& rows);
    /// Fritchii builds, global settings, docked: while FSR or DLSS upscales OoT, "Resolution" is
    /// the size the upscaler makes (4K, 1440p, 1080p) and "Quality" reads "input -> output", only
    /// as OoT's upscale path really runs. The choices set the Max Resolution add-on and the DLSS
    /// quality (ocarina_upscale.h, Settings::OcarinaMaxResolutionOn).
    void SetupPictureMenu(const std::map<u32, QWidget*>& rows);
    void UpdatePictureMenu();
    void ApplyPictureChoice(int choice);
    /// Fritchii builds: the window filter's pros and cons, and which one suits the picture size
    /// on this screen (bigger: Area, smaller: ScaleForce), under the "Window filter" row.
    void UpdateWindowFilterNote();
    /// The picture choices for the upscaler now picked (none while it's off).
    [[nodiscard]] std::vector<int> PictureChoicesNow() const;
    [[nodiscard]] int CurrentPictureChoice() const;
    /// Fritchii builds, global settings: one "Upscaler" choice under the resolution (Game, No
    /// sharpening, FSR 3.1, FSR 4, DLSS) that sets OoT's anti-aliasing and the DLSS checkbox,
    /// whose own rows are hidden; AMD's and NVIDIA's download panels and the DLSS options sit
    /// under it.
    void SetupUpscalerChoice(std::map<u32, QWidget*>& hold_graphics);
    void UpdateUpscalerChoice();
    void ApplyUpscalerChoice(int index);
    /// OoT's "DLSS jitter" row only while DLSS upscales OoT, the LOD bias row only with Custom
    /// texture sharpness. `rows`: the built setting rows by setting id.
    void SetupOcarinaTextureRows(const std::map<u32, QWidget*>& rows);

    void RetrieveVulkanDevices();

    /* Turns a Vulkan present mode into a textual string for a UI
     * (and eventually for a human to read) */
    const QString TranslateVSyncMode(VkPresentModeKHR mode,
                                     Settings::RendererBackend backend) const;

    Settings::RendererBackend GetCurrentGraphicsBackend() const;

    int FindIndex(u32 enumeration, int value) const;

    std::unique_ptr<Ui::ConfigureGraphics> ui;
    QColor bg_color;

    std::vector<std::function<void(bool)>> apply_funcs{};

    std::vector<VkDeviceInfo::Record>& records;
    std::vector<QString> vulkan_devices;
    std::vector<std::vector<VkPresentModeKHR>> device_present_modes;
    std::vector<VkPresentModeKHR>
        vsync_mode_combobox_enum_map{}; //< Keeps track of which present mode corresponds to which
                                        // selection in the combobox
    u32 vulkan_device{};
    const std::function<void()>& expose_compute_option;
    const std::function<void(Settings::AspectRatio, Settings::ResolutionSetup)> update_aspect_ratio;

    const Core::System& system;
    const ConfigurationShared::ComboboxTranslationMap& combobox_translations;

    QPushButton* api_restore_global_button;
    QComboBox* vulkan_device_combobox;
    QComboBox* api_combobox;
    QComboBox* vsync_mode_combobox;
    QPushButton* vsync_restore_global_button;
    QWidget* vulkan_device_widget;
    QWidget* api_widget;
    QComboBox* aspect_ratio_combobox;
    QComboBox* resolution_combobox;
    QComboBox* oot_aa_combobox{};          ///< The oot_anti_aliasing choice
    AmdFsr4Panel* amd_fsr4_panel{};       ///< Under it while AMD FSR 4 is picked
    QCheckBox* nvidia_dlss_checkbox{};     ///< The nvidia_dlss setting
    QWidget* nvidia_dlss_group{};          ///< The DLSS checkbox and NVIDIA's panel under it
    std::vector<QWidget*> dlss_rows;       ///< DLSS quality/model/jitter/game's own DLSS rows
    QComboBox* dlss_quality_combobox{};    ///< The nvidia_dlss_quality choice
    QWidget* resolution_widget{};          ///< The resolution_setup row
    QVBoxLayout* resolution_group_layout{}; ///< Fritchii: the resolution choice and what's under it
    QWidget* oot_aa_widget{};              ///< The oot_anti_aliasing row
    QWidget* nvidia_dlss_widget{};         ///< The nvidia_dlss checkbox row
    QComboBox* upscaler_combobox{};        ///< Fritchii: the "Upscaler" choice
    // Fritchii's picture menu (SetupPictureMenu).
    QWidget* dlss_quality_widget{};        ///< The nvidia_dlss_quality row (hidden there)
    QWidget* output_row{};                 ///< "Resolution" while an upscaler is on
    QComboBox* output_combobox{};
    QWidget* quality_row{};                ///< "Quality": input -> output
    QComboBox* quality_combobox{};
    QLabel* quality_fixed_label{};         ///< In place of the choice when there is one
    QLabel* upscale_note{};                ///< Where an upscaler starts from, and how to go lower
    QComboBox* scaling_filter_combobox{};  ///< The "Window filter" choice
    QLabel* filter_note{};                 ///< Fritchii: under it, pros, cons and the advice
    QWidget* texture_sharpness_widget{};
    QComboBox* texture_sharpness_combobox{};
    QWidget* lod_custom_widget{};
    bool max_resolution_choice{true};      ///< OoT's Max Resolution add-on, saved on Apply
    bool max_resolution_saved{true};       ///< ...as it was when the page opened
    bool max_resolution_when_off{true};    ///< Its value when the upscaler goes back to off
    u32 upscale_output_choice{};           ///< oot_upscale_output, saved on Apply (0 = Auto)
    u32 upscale_output_saved{};            ///< ...as it was when the page opened
};
