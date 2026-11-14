; kinoz Setup.exe — a thin single-file wrapper. It never shows an Inno wizard: it unpacks the app to a temp folder
; and starts kinoz's OWN setup window (kinoz.exe --installer), which is drawn by the app itself.
;   Build: scripts\release.ps1   (creates dist\payload.zip first, see scripts\make-payload.ps1)
#define AppName "kinoz"
#define AppVer "0.3.0"

[Setup]
AppId={{B3E1D3A8-5C2F-4F1E-9B6A-7D2C4E8A1F90}
AppName={#AppName}
AppVersion={#AppVer}
AppPublisher=P4inz Interactive Labs
AppPublisherURL=https://github.com/p4inz-code/kinoz
AppCopyright=Copyright (C) 2026 Atharva Patil. Free software under the GNU GPL v3.
VersionInfoVersion={#AppVer}
CreateAppDir=no
Uninstallable=no
DisableStartupPrompt=yes
DisableProgramGroupPage=yes
DisableDirPage=yes
DisableReadyPage=yes
DisableWelcomePage=yes
DisableFinishedPage=yes
OutputDir=..\dist\installer
OutputBaseFilename=kinoz-Setup-{#AppVer}
SetupIconFile=..\resources\app.ico
Compression=none
SolidCompression=no
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible

[Files]
Source: "..\dist\payload.zip"; DestDir: "{tmp}"; Flags: dontcopy

[Code]
function InitializeSetup(): Boolean;
var
  Rc: Integer;
  Dir, Zip, Args: String;
begin
  Result := False;                         // returning False ends Setup without ever showing Inno's own screens
  ExtractTemporaryFile('payload.zip');
  Zip := ExpandConstant('{tmp}\payload.zip');
  Dir := ExpandConstant('{tmp}\kinoz-setup');
  ForceDirectories(Dir);
  if (not Exec(ExpandConstant('{sys}\tar.exe'), '-xf "' + Zip + '" -C "' + Dir + '"', '', SW_HIDE, ewWaitUntilTerminated, Rc)) or (Rc <> 0) then
  begin
    MsgBox('kinoz could not unpack its setup files. Your PC was not changed.', mbError, MB_OK);
    Exit;
  end;
  Args := '--installer';
  if WizardSilent then Args := Args + ' --silent';      // updates run silently, no window
  if not Exec(Dir + '\kinoz.exe', Args, Dir, SW_SHOW, ewWaitUntilTerminated, Rc) then
    MsgBox('kinoz setup could not start. Your PC was not changed.', mbError, MB_OK);
end;
