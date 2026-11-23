; Rata Workspace — Per-User Windows 11 Installer (Inno Setup 6)
; Compiles only in CI / release packaging; never linked into the app.
; Installs per-user without admin privileges and upgrades cleanly in place (fixed AppId).
; Adds {app} to per-user HKCU PATH so `rata-workspace --update` / `--set-token` work from PowerShell.
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
ChangesEnvironment=yes

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

[Registry]
; Add {app} to per-user PATH (HKCU, no admin rights) idempotently.
Root: HKCU; Subkey: "Environment"; ValueType: expandsz; ValueName: "Path"; ValueData: "{olddata};{app}"; Check: NeedsAddPath(ExpandConstant('{app}'))

[Run]
; Runs after both interactive and /VERYSILENT auto-update installs so the updated app relaunches.
Filename: "{app}\rata-workspace.exe"; Description: "{cm:LaunchProgram,Rata Workspace}"; Flags: nowait postinstall

[Code]
function NeedsAddPath(Param: string): boolean;
var
  OrigPath: string;
begin
  if not RegQueryStringValue(HKEY_CURRENT_USER, 'Environment', 'Path', OrigPath) then
  begin
    Result := True;
    exit;
  end;
  Result := Pos(';' + Uppercase(Param) + ';', ';' + Uppercase(OrigPath) + ';') = 0;
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var
  OrigPath, AppDir, NewPath: string;
  P: Integer;
begin
  if CurUninstallStep = usPostUninstall then
  begin
    if RegQueryStringValue(HKEY_CURRENT_USER, 'Environment', 'Path', OrigPath) then
    begin
      AppDir := ExpandConstant('{app}');
      NewPath := ';' + OrigPath + ';';
      P := Pos(';' + Uppercase(AppDir) + ';', Uppercase(NewPath));
      if P > 0 then
      begin
        Delete(NewPath, P, Length(AppDir) + 1);
        if (Length(NewPath) > 0) and (NewPath[1] = ';') then
          Delete(NewPath, 1, 1);
        if (Length(NewPath) > 0) and (NewPath[Length(NewPath)] = ';') then
          Delete(NewPath, Length(NewPath), 1);
        RegWriteExpandStringValue(HKEY_CURRENT_USER, 'Environment', 'Path', NewPath);
      end;
    end;
  end;
end;
