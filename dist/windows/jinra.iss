; SPDX-License-Identifier: GPL-3.0-or-later
#ifndef PayloadDir
  #error PayloadDir must point to the clean Jinra package folder.
#endif
#ifndef OutputDir
  #define OutputDir "."
#endif
#ifndef AppVersion
  #define AppVersion "0.1.0-preview.1"
#endif

[Setup]
AppId={{5C9CB795-2BD3-4954-B7AE-9B0651B06A61}
AppName=Jinra
AppVersion={#AppVersion}
AppVerName=Jinra {#AppVersion}
AppPublisher=JinWooPT
AppPublisherURL=https://github.com/JinWooPT/Jinra
AppSupportURL=https://github.com/JinWooPT/Jinra/issues
AppUpdatesURL=https://github.com/JinWooPT/Jinra/releases
DefaultDirName={localappdata}\Programs\Jinra
DefaultGroupName=Jinra
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
UninstallDisplayIcon={app}\jinra.exe
OutputDir={#OutputDir}
OutputBaseFilename=Jinra-Setup-{#AppVersion}-x64
SetupIconFile=..\eden.ico
LicenseFile=..\..\LICENSE.txt
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
CloseApplications=yes
CloseApplicationsFilter=jinra.exe
RestartApplications=no
#ifdef TestMode
CreateUninstallRegKey=no
#endif

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Shortcuts:"; Flags: unchecked

[Dirs]
Name: "{app}\user"; Flags: uninsneveruninstall

[Files]
Source: "{#PayloadDir}\jinra.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#PayloadDir}\LICENSE.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#PayloadDir}\GETTING_STARTED.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#PayloadDir}\LICENSES\*"; DestDir: "{app}\LICENSES"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#PayloadDir}\*.dll"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#PayloadDir}\plugins\*"; DestDir: "{app}\plugins"; Flags: ignoreversion recursesubdirs createallsubdirs skipifsourcedoesntexist

#ifndef TestMode
[Icons]
Name: "{group}\Jinra"; Filename: "{app}\jinra.exe"; WorkingDir: "{app}"
Name: "{group}\Uninstall Jinra"; Filename: "{uninstallexe}"
Name: "{autodesktop}\Jinra"; Filename: "{app}\jinra.exe"; WorkingDir: "{app}"; Tasks: desktopicon

[Run]
Filename: "{app}\jinra.exe"; Description: "Launch Jinra"; Flags: nowait postinstall skipifsilent
#endif
