// Shader preparation adapted from Fritchii v6 by Frizti and contributors.
// SPDX-License-Identifier: GPL-3.0-or-later

#include "common/logging.h"
#include "yuzu/fritchii/precompile_gate.h"
#include "yuzu/main_window.h"

void MainWindow::SetUpShaderPreparation() {
    // "Preparing shaders" before the game starts when its precompiled pipelines need preparing.
    auto* gate = new FritchiiUi::PrecompileGate(
        this, statusBar(),
        {.pause =
             [this] {
                 LOG_INFO(Frontend, "Pausing: shader preparation");
                 OnPauseGame();
             },
         .resume = [this] { OnStartGame(); }});
    connect(this, &MainWindow::EmulationStarting, gate,
            &FritchiiUi::PrecompileGate::OnEmulationStarting);
    connect(this, &MainWindow::EmulationStopping, gate,
            &FritchiiUi::PrecompileGate::OnEmulationStopping);
}
