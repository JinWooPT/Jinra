// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2016 Citra Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <memory>
#include <QCoreApplication>
#include <QIcon>
#include "common/logging.h"
#include "common/settings.h"
#include "common/settings_enums.h"
#include "core/core.h"
#include "qt_common/config/uisettings.h"
#include "qt_common/util/vk.h"
#include "ui_configure.h"
#include "yuzu/configuration/configure_applets.h"
#include "yuzu/configuration/configure_audio.h"
#include "yuzu/configuration/configure_cpu.h"
#include "yuzu/configuration/configure_debug_tab.h"
#include "yuzu/configuration/configure_dialog.h"
#include "yuzu/configuration/configure_filesystem.h"
#include "yuzu/configuration/configure_general.h"
#include "yuzu/configuration/configure_graphics.h"
#include "yuzu/configuration/configure_graphics_advanced.h"
#include "yuzu/configuration/configure_graphics_extensions.h"
#include "yuzu/configuration/configure_hotkeys.h"
#include "yuzu/configuration/configure_input.h"
#include "yuzu/configuration/configure_input_player.h"
#include "yuzu/configuration/configure_network.h"
#include "yuzu/configuration/configure_profile_manager.h"
#include "yuzu/configuration/configure_system.h"
#include "yuzu/configuration/configure_ui.h"
#include "yuzu/configuration/configure_web.h"
#include "yuzu/hotkeys.h"

ConfigurationPages::ConfigurationPages(QWidget* parent, HotkeyRegistry& registry_,
                                       InputCommon::InputSubsystem* input_subsystem_,
                                       std::vector<VkDeviceInfo::Record>& vk_device_records_,
                                       Core::System& system_, bool enable_web_config_)
    : QObject(parent), registry(registry_), system(system_), input_subsystem(input_subsystem_),
      vk_device_records(vk_device_records_), enable_web_config(enable_web_config_),
      staging{std::make_unique<QWidget>(parent)},
      builder{std::make_unique<ConfigurationShared::Builder>(staging.get(),
                                                             !system_.IsPoweredOn())} {
    // Some settings call setVisible() before they are added to a page.
    staging->hide();
    Settings::SetConfiguringGlobal(true);
}

QList<QWidget*> ConfigurationPages::LoadCategory(int category) {
    auto* owner = staging.get();
    switch (category) {
    case 0:
        if (!general_tab) {
            general_tab = std::make_unique<ConfigureGeneral>(system, nullptr, *builder, owner);
            hotkeys_tab = std::make_unique<ConfigureHotkeys>(system.HIDCore(), owner);
            ui_tab = std::make_unique<ConfigureUi>(system, owner);
            web_tab = std::make_unique<ConfigureWeb>(owner);
            debug_tab_tab = std::make_unique<ConfigureDebugTab>(system, owner);
            if (screenshot_info) {
                ui_tab->UpdateScreenshotInfo(screenshot_info->first, screenshot_info->second);
            }
            web_tab->SetWebServiceConfigEnabled(enable_web_config);
            hotkeys_tab->Populate(registry);
            general_tab->SetResetCallback([this] { emit ResetRequested(); });
            connect(ui_tab.get(), &ConfigureUi::LanguageChanged, this, [this](const QString& locale) {
                emit LanguageChanged(locale);
                UISettings::values.is_game_list_reload_pending = true;
                ApplyConfiguration();
            });
            connect(general_tab.get(), &ConfigureGeneral::ExternalContentDirsChanged, this,
                    &ConfigurationPages::ExternalContentDirsChanged);
        }
        return {general_tab.get(), hotkeys_tab.get(), ui_tab.get(), web_tab.get(),
                debug_tab_tab.get()};
    case 1:
        if (!system_tab) {
            system_tab = std::make_unique<ConfigureSystem>(system, nullptr, *builder, owner);
            profile_tab = std::make_unique<ConfigureProfileManager>(system, owner);
            network_tab = std::make_unique<ConfigureNetwork>(system, owner);
            filesystem_tab = std::make_unique<ConfigureFilesystem>(owner);
            applets_tab = std::make_unique<ConfigureApplets>(system, nullptr, *builder, owner);
        }
        return {system_tab.get(), profile_tab.get(), network_tab.get(), filesystem_tab.get(),
                applets_tab.get()};
    case 2:
        if (!cpu_tab) {
            cpu_tab = std::make_unique<ConfigureCpu>(system, nullptr, *builder, owner);
        }
        return {cpu_tab.get()};
    case 3:
        if (!graphics_tab) {
            graphics_advanced_tab =
                std::make_unique<ConfigureGraphicsAdvanced>(system, nullptr, *builder, owner);
            graphics_extensions_tab =
                std::make_unique<ConfigureGraphicsExtensions>(system, nullptr, *builder, owner);
            graphics_tab = std::make_unique<ConfigureGraphics>(
                system, vk_device_records, [this] { graphics_advanced_tab->ExposeComputeOption(); },
                [this](Settings::AspectRatio ratio, Settings::ResolutionSetup setup) {
                    screenshot_info.emplace(ratio, setup);
                    if (ui_tab) {
                        ui_tab->UpdateScreenshotInfo(ratio, setup);
                    }
                },
                nullptr, *builder, owner);
        }
        return {graphics_tab.get(), graphics_advanced_tab.get(), graphics_extensions_tab.get()};
    case 4:
        if (!audio_tab) {
            audio_tab = std::make_unique<ConfigureAudio>(system, nullptr, *builder, owner);
        }
        return {audio_tab.get()};
    case 5:
        if (!input_tab) {
            input_tab = std::make_unique<ConfigureInput>(system, owner);
            input_tab->Initialize(input_subsystem);
            if (home_layout) {
                input_tab->UseHomeLayout();
            }
            input_tab->hide();
        }
        return input_tab->GetSubTabs();
    default:
        return {};
    }
}

ConfigurationPages::~ConfigurationPages() = default;

ConfigureDialog::ConfigureDialog(QWidget* parent, HotkeyRegistry& registry,
                                 InputCommon::InputSubsystem* input_subsystem,
                                 std::vector<VkDeviceInfo::Record>& vk_device_records,
                                 Core::System& system, bool enable_web_config)
    : QDialog(parent), ui{std::make_unique<Ui::ConfigureDialog>()},
      settings(this, registry, input_subsystem, vk_device_records, system, enable_web_config) {
    ui->setupUi(this);
    setWindowTitle(QStringLiteral("Jinra — Settings"));
    setWindowIcon(QIcon(QStringLiteral(":/img/jinra.ico")));
    ui->selectorList->setMinimumWidth(140);
    ui->selectorList->setMaximumWidth(160);

    for (int category = 0; category < ConfigurationPages::CategoryCount; ++category) {
        for (auto* page : settings.LoadCategory(category)) {
            ui->tabWidget->addTab(page, page->accessibleName());
        }
    }
    PopulateSelectionList();

    connect(ui->tabWidget, &QTabWidget::currentChanged, this, [this](int index) {
        if (index != -1) {
            settings.ResetDebugTab();
        }
    });
    connect(&settings, &ConfigurationPages::LanguageChanged, this, &ConfigureDialog::LanguageChanged);
    connect(&settings, &ConfigurationPages::ResetRequested, this, &QDialog::reject);
    connect(&settings, &ConfigurationPages::ExternalContentDirsChanged, this,
            &ConfigureDialog::ExternalContentDirsChanged);
    connect(ui->selectorList, &QListWidget::itemSelectionChanged, this,
            &ConfigureDialog::UpdateVisibleTabs);

    if (system.IsPoweredOn()) {
        QPushButton* apply_button = ui->buttonBox->addButton(QDialogButtonBox::Apply);
        connect(apply_button, &QAbstractButton::clicked, this,
                &ConfigureDialog::HandleApplyButtonClicked);
    }

    adjustSize();
    ui->selectorList->setCurrentRow(0);

    // Selects the leftmost button on the bottom bar (Cancel as of writing)
    ui->buttonBox->setFocus();
}

ConfigureDialog::~ConfigureDialog() = default;

void ConfigureDialog::SelectCategory(int index) {
    ui->selectorList->setCurrentRow(std::clamp(index, 0, ui->selectorList->count() - 1));
}

void ConfigureDialog::ApplyConfiguration() {
    settings.ApplyConfiguration();
}

void ConfigurationPages::ApplyConfiguration() {
    const auto apply = [](const auto& tab) {
        if (tab) {
            tab->ApplyConfiguration();
        }
    };
    apply(general_tab);
    apply(ui_tab);
    apply(system_tab);
    apply(profile_tab);
    apply(filesystem_tab);
    apply(input_tab);
    if (hotkeys_tab) {
        hotkeys_tab->ApplyConfiguration(registry);
    }
    apply(cpu_tab);
    apply(graphics_tab);
    apply(graphics_advanced_tab);
    apply(graphics_extensions_tab);
    apply(audio_tab);
    apply(debug_tab_tab);
    apply(web_tab);
    apply(network_tab);
    apply(applets_tab);
    system.ApplySettings();
    Settings::LogSettings();
}

void ConfigureDialog::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) {
        RetranslateUI();
    }

    QDialog::changeEvent(event);
}

void ConfigureDialog::RetranslateUI() {
    const int old_row = ui->selectorList->currentRow();
    const int old_index = ui->tabWidget->currentIndex();

    ui->retranslateUi(this);

    PopulateSelectionList();
    ui->selectorList->setCurrentRow(old_row);

    UpdateVisibleTabs();
    ui->tabWidget->setCurrentIndex(old_index);
}

void ConfigureDialog::HandleApplyButtonClicked() {
    UISettings::values.configuration_applied = true;
    ApplyConfiguration();
}

void ConfigurationPages::ResetDebugTab() {
    if (debug_tab_tab) {
        debug_tab_tab->SetCurrentIndex(0);
    }
}

std::array<QString, ConfigurationPages::CategoryCount> ConfigurationPages::CategoryTitles() {
    const auto name = [](const char* text) {
        return QCoreApplication::translate("ConfigureDialog", text);
    };
    return {
        name(QT_TRANSLATE_NOOP("ConfigureDialog", "General")),
        name(QT_TRANSLATE_NOOP("ConfigureDialog", "System")),
        name(QT_TRANSLATE_NOOP("ConfigureDialog", "CPU")),
        name(QT_TRANSLATE_NOOP("ConfigureDialog", "Graphics")),
        name(QT_TRANSLATE_NOOP("ConfigureDialog", "Audio")),
        name(QT_TRANSLATE_NOOP("ConfigureDialog", "Controls")),
    };
}

Q_DECLARE_METATYPE(QList<QWidget*>);

void ConfigureDialog::PopulateSelectionList() {
    [[maybe_unused]] const QSignalBlocker blocker(ui->selectorList);

    ui->selectorList->clear();
    const auto categories = ConfigurationPages::CategoryTitles();
    for (int category = 0; category < ConfigurationPages::CategoryCount; ++category) {
        auto* const item = new QListWidgetItem(categories[category]);
        item->setData(Qt::UserRole, QVariant::fromValue(settings.LoadCategory(category)));

        ui->selectorList->addItem(item);
    }
}

void ConfigureDialog::UpdateVisibleTabs() {
    const auto items = ui->selectorList->selectedItems();
    if (items.isEmpty()) {
        return;
    }

    [[maybe_unused]] const QSignalBlocker blocker(ui->tabWidget);

    ui->tabWidget->clear();

    const auto tabs = qvariant_cast<QList<QWidget*>>(items[0]->data(Qt::UserRole));

    for (auto* const tab : tabs) {
        LOG_DEBUG(Frontend, "{}", tab->accessibleName().toStdString());
        ui->tabWidget->addTab(tab, tab->accessibleName());
    }
}
