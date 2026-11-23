; Rata Workspace — Per-User Windows 11 Installer (Inno Setup 6)
; Compiles only in CI / release packaging; never linked into the app.
; Installs per-user without admin privileges and upgrades cleanly in place (fixed AppId).
; User settings and projects live in %APPDATA%\RataWorkspace and are never touched on upgrade/uninstall.

#ifndef RATA_APP_VERSION
  #define RATA_APP_VERSION "0.1.0"
#endif
#ifndef RATA_STAGE_DIR
  #define RATA_STAGE_DIR "..\..\build\windows-stage"
#endif
#ifndef RATA_OUT_DIR
  #define RATA_OUT_DIR "..\..\build\release-artifacts"
#endif

[Setup]
AppId={{8F5E3B9A-6C4D-4E7A-9B2F-3D1A7E5C8B42}
AppName=Rata Workspace
AppVersion={#RATA_APP_VERSION}
AppVerName=Rata Workspace {#RATA_APP_VERSION}
AppPublisher=Rata Workspace
AppSupportURL=https://github.com/omerd2905/rata-workspace
DefaultDirName={localappdata}\Programs\RataWorkspace
DefaultGroupName=Rata Workspace
AllowNoIcons=yes
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=commandline
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
SetupIconFile=..\..\assets\rata-workspace.ico
UninstallDisplayIcon={app}\rata-workspace.exe
OutputDir={#RATA_OUT_DIR}
OutputBaseFilename=rata-workspace-v{#RATA_APP_VERSION}-windows-x86_64-setup
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
CloseApplications=yes
DisableProgramGroupPage=yes

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "{#RATA_STAGE_DIR}\rata-workspace.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#RATA_STAGE_DIR}\licenses\*"; DestDir: "{app}\licenses"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\Rata Workspace"; Filename: "{app}\rata-workspace.exe"
Name: "{autodesktop}\Rata Workspace"; Filename: "{app}\rata-workspace.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\rata-workspace.exe"; Description: "{cm:LaunchProgram,Rata Workspace}"; Flags: nowait postinstall skipifsilent
