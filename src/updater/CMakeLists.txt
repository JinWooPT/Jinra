# SPDX-FileCopyrightText: Copyright 2026 Jinra Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

add_executable(jinra-updater main.cpp ../frontend_common/update_installer.cpp)
target_include_directories(jinra-updater PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_compile_features(jinra-updater PRIVATE cxx_std_20)
target_link_libraries(jinra-updater PRIVATE user32 shell32)
set_target_properties(jinra-updater PROPERTIES WIN32_EXECUTABLE ON)

# The helper runs from a temporary folder, away from the DLLs next to jinra.exe.
if (MSVC)
    set_property(TARGET jinra-updater PROPERTY MSVC_RUNTIME_LIBRARY
                 "MultiThreaded$<$<CONFIG:Debug>:Debug>")
else()
    target_link_options(jinra-updater PRIVATE -static)
endif()
