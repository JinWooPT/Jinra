// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <QIcon>
#include "common/scm_rev.h"
#include "ui_aboutdialog.h"
#include "yuzu/about_dialog.h"

AboutDialog::AboutDialog(QWidget* parent)
    : QDialog(parent), ui{std::make_unique<Ui::AboutDialog>()} {
    ui->setupUi(this);
    // Try and request the icon from Qt theme (Linux?)
    const QIcon yuzu_logo = QIcon::fromTheme(QStringLiteral("io.github.JinWooPT.jinra"),
                                           QIcon(QStringLiteral(":/img/jinra.ico")));
    if (!yuzu_logo.isNull()) {
        ui->labelLogo->setPixmap(yuzu_logo.pixmap(200));
    }
    const auto build = QStringLiteral("%1 %2 | %3")
                           .arg(QString::fromUtf8(Common::g_build_name),
                                QString::fromUtf8(Common::g_build_version),
                                QString::fromUtf8(Common::g_compiler_id));
    ui->labelBuildInfo->setText(ui->labelBuildInfo->text().arg(
        build, QString::fromUtf8(Common::g_build_date).left(10)));
}

AboutDialog::~AboutDialog() = default;
