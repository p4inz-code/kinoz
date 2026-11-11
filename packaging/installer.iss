; Inno Setup script — builds the normal Windows "Setup.exe" installer.   Run: scripts\release.ps1
#define AppName "kinoz"
#define AppExe "kinoz.exe"
#define AppVer "0.1.0"

[Setup]
AppId={{B3E1D3A8-5C2F-4F1E-9B6A-7D2C4E8A1F90}
AppName={#AppName}
AppVersion={#AppVer}
AppVerName={#AppName} {#AppVer}
AppPublisher=P4inz Interactive Labs
AppPublisherURL=https://github.com/p4inz-code/kinoz
AppSupportURL=https://github.com/p4inz-code/kinoz/issues
AppUpdatesURL=https://github.com/p4inz-code/kinoz/releases
AppCopyright=Copyright (C) 2026 Atharva Patil. Free software under the GNU GPL v3.
VersionInfoVersion={#AppVer}
DefaultDirName={code:DefaultInstallDir}
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes
LicenseFile=..\LICENSE
OutputDir=..\dist\installer
OutputBaseFilename=kinoz-Setup-{#AppVer}
SetupIconFile=..\resources\app.ico
UninstallDisplayIcon={app}\{#AppExe}
Compression=lzma2/fast
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
CloseApplications=yes
RestartApplications=no
UsePreviousAppDir=yes

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "Create a &desktop shortcut"; GroupDescription: "Shortcuts:"
Name: "openwith"; Description: "Add {#AppName} to the ""Open with"" menu for video, audio, image and GIF files"; GroupDescription: "File types:"

[Files]
Source: "..\dist\app\*"; DestDir: "{app}"; Excludes: "data\*,*.pdb,*.ilk,vc_redist.x64.exe"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\dist\app\vc_redist.x64.exe"; DestDir: "{tmp}"; Flags: deleteafterinstall; Check: NeedsVCRuntime
Source: "..\README.md"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\{#AppName}"; Filename: "{app}\{#AppExe}"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#AppExe}"; Tasks: desktopicon

[Registry]
Root: HKA; Subkey: "Software\Classes\Applications\{#AppExe}"; ValueType: string; ValueName: "FriendlyAppName"; ValueData: "{#AppName}"; Tasks: openwith; Flags: uninsdeletekey
Root: HKA; Subkey: "Software\Classes\Applications\{#AppExe}\shell\open\command"; ValueType: string; ValueData: """{app}\{#AppExe}"" ""%1"""; Tasks: openwith
Root: HKA; Subkey: "Software\Classes\kinoz.Media"; ValueType: string; ValueData: "{#AppName} media file"; Tasks: openwith; Flags: uninsdeletekey
Root: HKA; Subkey: "Software\Classes\kinoz.Media\DefaultIcon"; ValueType: string; ValueData: "{app}\{#AppExe},0"; Tasks: openwith
Root: HKA; Subkey: "Software\Classes\kinoz.Media\shell\open\command"; ValueType: string; ValueData: """{app}\{#AppExe}"" ""%1"""; Tasks: openwith
#define Ext(E) "Root: HKA; Subkey: ""Software\Classes\." + E + "\OpenWithProgids""; ValueType: string; ValueName: ""kinoz.Media""; ValueData: """"; Tasks: openwith; Flags: uninsdeletevalue"
#emit Ext("mp4")
#emit Ext("mkv")
#emit Ext("avi")
#emit Ext("webm")
#emit Ext("mov")
#emit Ext("wmv")
#emit Ext("m4v")
#emit Ext("flv")
#emit Ext("mpg")
#emit Ext("ts")
#emit Ext("mp3")
#emit Ext("flac")
#emit Ext("wav")
#emit Ext("ogg")
#emit Ext("opus")
#emit Ext("m4a")
#emit Ext("aac")
#emit Ext("jpg")
#emit Ext("jpeg")
#emit Ext("png")
#emit Ext("gif")
#emit Ext("bmp")
#emit Ext("webp")

[Run]
Filename: "{tmp}\vc_redist.x64.exe"; Parameters: "/install /quiet /norestart"; StatusMsg: "Installing the Visual C++ runtime..."; Check: NeedsVCRuntime; Flags: waituntilterminated
Filename: "{app}\{#AppExe}"; Description: "Launch {#AppName}"; Flags: nowait postinstall skipifsilent

[Code]
function GetDriveTypeW(lpRootPathName: String): Cardinal;
  external 'GetDriveTypeW@kernel32.dll stdcall';

// A fixed (non-removable) drive other than C: if there is one, so nothing lands on C: by default.
// Falls back to the normal per-user programs folder. You can still pick any folder.
function IsFixedDrive(const Root: String): Boolean;
begin
  Result := DirExists(Root) and (GetDriveTypeW(Root) = 3);
end;

function DefaultInstallDir(Param: String): String;
begin
  if IsFixedDrive('F:\') then Result := 'F:\Programs\{#AppName}'
  else if IsFixedDrive('D:\') then Result := 'D:\Programs\{#AppName}'
  else Result := ExpandConstant('{autopf}\{#AppName}');
end;

function NeedsVCRuntime: Boolean;
var Installed: Cardinal;
begin
  Result := not (RegQueryDWordValue(HKLM, 'SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x64', 'Installed', Installed) and (Installed = 1));
end;
