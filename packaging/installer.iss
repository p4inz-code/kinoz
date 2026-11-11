; Inno Setup script â€” builds the kinoz Setup.exe: a short, dark, branded wizard (welcome, folder, install, finish).
;   Run: scripts\release.ps1
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
DisableReadyPage=yes
DisableDirPage=no
ShowLanguageDialog=no
OutputDir=..\dist\installer
OutputBaseFilename=kinoz-Setup-{#AppVer}
SetupIconFile=..\resources\app.ico
UninstallDisplayIcon={app}\{#AppExe}
Compression=lzma2/fast
SolidCompression=yes
; look: modern wizard, always dark, with kinoz artwork instead of the grey Windows panels
WizardStyle=modern dynamic
WizardImageFile=wizard\side-100.png,wizard\side-150.png,wizard\side-200.png
WizardSmallImageFile=wizard\small-100.png,wizard\small-150.png,wizard\small-200.png
WizardSizePercent=100
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
CloseApplications=yes
RestartApplications=no
UsePreviousAppDir=yes

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Messages]
SetupWindowTitle=%1
WelcomeLabel1=Welcome to kinoz
WelcomeLabel2=A calm, fast, offline player for videos, music, images and GIFs.%n%nThis installs kinoz {#AppVer}. It is free software under the GNU GPL v3, made by Atharva Patil, P4inz Interactive Labs.
SelectDirDesc=Where should kinoz live?
SelectDirLabel3=kinoz and everything it saves will stay inside this folder.
SelectDirBrowseLabel=Click Next to continue, or Browse to pick another folder.
InstallingLabel=Installing kinozâ€¦
FinishedHeadingLabel=kinoz is ready
FinishedLabelNoIcons=Everything is in place. Drop a folder onto kinoz and press play.
FinishedLabel=Everything is in place. Drop a folder onto kinoz and press play.
ClickFinish=Click Finish to close this window.

[Files]
Source: "..\dist\app\*"; DestDir: "{app}"; Excludes: "data\*,*.pdb,*.ilk,vc_redist.x64.exe"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\dist\app\vc_redist.x64.exe"; DestDir: "{tmp}"; Flags: deleteafterinstall; Check: NeedsVCRuntime
Source: "..\README.md"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\{#AppName}"; Filename: "{app}\{#AppExe}"

[Registry]
; adds kinoz to the "Open with" list of media files (it does not take over your default player)
Root: HKA; Subkey: "Software\Classes\Applications\{#AppExe}"; ValueType: string; ValueName: "FriendlyAppName"; ValueData: "{#AppName}"; Flags: uninsdeletekey
Root: HKA; Subkey: "Software\Classes\Applications\{#AppExe}\shell\open\command"; ValueType: string; ValueData: """{app}\{#AppExe}"" ""%1"""
Root: HKA; Subkey: "Software\Classes\kinoz.Media"; ValueType: string; ValueData: "{#AppName} media file"; Flags: uninsdeletekey
Root: HKA; Subkey: "Software\Classes\kinoz.Media\DefaultIcon"; ValueType: string; ValueData: "{app}\{#AppExe},0"
Root: HKA; Subkey: "Software\Classes\kinoz.Media\shell\open\command"; ValueType: string; ValueData: """{app}\{#AppExe}"" ""%1"""
#define Ext(E) "Root: HKA; Subkey: ""Software\Classes\." + E + "\OpenWithProgids""; ValueType: string; ValueName: ""kinoz.Media""; ValueData: """"; Flags: uninsdeletevalue"
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

; right-click menu: 'Play with kinoz' on media files and 'Open folder in kinoz' on folders (removed again on uninstall)
Root: HKA; Subkey: "Software\Classes\SystemFileAssociations\.mp4\shell\kinoz"; ValueType: string; ValueName: "MUIVerb"; ValueData: "Play with kinoz"; Flags: uninsdeletekey
Root: HKA; Subkey: "Software\Classes\SystemFileAssociations\.mp4\shell\kinoz"; ValueType: string; ValueName: "Icon"; ValueData: "{app}\{#AppExe},0"
Root: HKA; Subkey: "Software\Classes\SystemFileAssociations\.mp4\shell\kinoz\command"; ValueType: string; ValueData: """{app}\{#AppExe}"" ""%1"""
Root: HKA; Subkey: "Software\Classes\SystemFileAssociations\.mkv\shell\kinoz"; ValueType: string; ValueName: "MUIVerb"; ValueData: "Play with kinoz"; Flags: uninsdeletekey
Root: HKA; Subkey: "Software\Classes\SystemFileAssociations\.mkv\shell\kinoz"; ValueType: string; ValueName: "Icon"; ValueData: "{app}\{#AppExe},0"
Root: HKA; Subkey: "Software\Classes\SystemFileAssociations\.mkv\shell\kinoz\command"; ValueType: string; ValueData: """{app}\{#AppExe}"" ""%1"""
Root: HKA; Subkey: "Software\Classes\SystemFileAssociations\.avi\shell\kinoz"; ValueType: string; ValueName: "MUIVerb"; ValueData: "Play with kinoz"; Flags: uninsdeletekey
Root: HKA; Subkey: "Software\Classes\SystemFileAssociations\.avi\shell\kinoz"; ValueType: string; ValueName: "Icon"; ValueData: "{app}\{#AppExe},0"
Root: HKA; Subkey: "Software\Classes\SystemFileAssociations\.avi\shell\kinoz\command"; ValueType: string; ValueData: """{app}\{#AppExe}"" ""%1"""
Root: HKA; Subkey: "Software\Classes\SystemFileAssociations\.webm\shell\kinoz"; ValueType: string; ValueName: "MUIVerb"; ValueData: "Play with kinoz"; Flags: uninsdeletekey
Root: HKA; Subkey: "Software\Classes\SystemFileAssociations\.webm\shell\kinoz"; ValueType: string; ValueName: "Icon"; ValueData: "{app}\{#AppExe},0"
Root: HKA; Subkey: "Software\Classes\SystemFileAssociations\.webm\shell\kinoz\command"; ValueType: string; ValueData: """{app}\{#AppExe}"" ""%1"""
Root: HKA; Subkey: "Software\Classes\SystemFileAssociations\.mov\shell\kinoz"; ValueType: string; ValueName: "MUIVerb"; ValueData: "Play with kinoz"; Flags: uninsdeletekey
Root: HKA; Subkey: "Software\Classes\SystemFileAssociations\.mov\shell\kinoz"; ValueType: string; ValueName: "Icon"; ValueData: "{app}\{#AppExe},0"
Root: HKA; Subkey: "Software\Classes\SystemFileAssociations\.mov\shell\kinoz\command"; ValueType: string; ValueData: """{app}\{#AppExe}"" ""%1"""
Root: HKA; Subkey: "Software\Classes\SystemFileAssociations\.wmv\shell\kinoz"; ValueType: string; ValueName: "MUIVerb"; ValueData: "Play with kinoz"; Flags: uninsdeletekey
Root: HKA; Subkey: "Software\Classes\SystemFileAssociations\.wmv\shell\kinoz"; ValueType: string; ValueName: "Icon"; ValueData: "{app}\{#AppExe},0"
Root: HKA; Subkey: "Software\Classes\SystemFileAssociations\.wmv\shell\kinoz\command"; ValueType: string; ValueData: """{app}\{#AppExe}"" ""%1"""
Root: HKA; Subkey: "Software\Classes\SystemFileAssociations\.m4v\shell\kinoz"; ValueType: string; ValueName: "MUIVerb"; ValueData: "Play with kinoz"; Flags: uninsdeletekey
Root: HKA; Subkey: "Software\Classes\SystemFileAssociations\.m4v\shell\kinoz"; ValueType: string; ValueName: "Icon"; ValueData: "{app}\{#AppExe},0"
Root: HKA; Subkey: "Software\Classes\SystemFileAssociations\.m4v\shell\kinoz\command"; ValueType: string; ValueData: """{app}\{#AppExe}"" ""%1"""
Root: HKA; Subkey: "Software\Classes\SystemFileAssociations\.flv\shell\kinoz"; ValueType: string; ValueName: "MUIVerb"; ValueData: "Play with kinoz"; Flags: uninsdeletekey
Root: HKA; Subkey: "Software\Classes\SystemFileAssociations\.flv\shell\kinoz"; ValueType: string; ValueName: "Icon"; ValueData: "{app}\{#AppExe},0"
Root: HKA; Subkey: "Software\Classes\SystemFileAssociations\.flv\shell\kinoz\command"; ValueType: string; ValueData: """{app}\{#AppExe}"" ""%1"""
Root: HKA; Subkey: "Software\Classes\SystemFileAssociations\.mpg\shell\kinoz"; ValueType: string; ValueName: "MUIVerb"; ValueData: "Play with kinoz"; Flags: uninsdeletekey
Root: HKA; Subkey: "Software\Classes\SystemFileAssociations\.mpg\shell\kinoz"; ValueType: string; ValueName: "Icon"; ValueData: "{app}\{#AppExe},0"
Root: HKA; Subkey: "Software\Classes\SystemFileAssociations\.mpg\shell\kinoz\command"; ValueType: string; ValueData: """{app}\{#AppExe}"" ""%1"""
Root: HKA; Subkey: "Software\Classes\SystemFileAssociations\.mpeg\shell\kinoz"; ValueType: string; ValueName: "MUIVerb"; ValueData: "Play with kinoz"; Flags: uninsdeletekey
Root: HKA; Subkey: "Software\Classes\SystemFileAssociations\.mpeg\shell\kinoz"; ValueType: string; ValueName: "Icon"; ValueData: "{app}\{#AppExe},0"
Root: HKA; Subkey: "Software\Classes\SystemFileAssociations\.mpeg\shell\kinoz\command"; ValueType: string; ValueData: """{app}\{#AppExe}"" ""%1"""
Root: HKA; Subkey: "Software\Classes\SystemFileAssociations\.ts\shell\kinoz"; ValueType: string; ValueName: "MUIVerb"; ValueData: "Play with kinoz"; Flags: uninsdeletekey
Root: HKA; Subkey: "Software\Classes\SystemFileAssociations\.ts\shell\kinoz"; ValueType: string; ValueName: "Icon"; ValueData: "{app}\{#AppExe},0"
Root: HKA; Subkey: "Software\Classes\SystemFileAssociations\.ts\shell\kinoz\command"; ValueType: string; ValueData: """{app}\{#AppExe}"" ""%1"""
Root: HKA; Subkey: "Software\Classes\SystemFileAssociations\.m2ts\shell\kinoz"; ValueType: string; ValueName: "MUIVerb"; ValueData: "Play with kinoz"; Flags: uninsdeletekey
Root: HKA; Subkey: "Software\Classes\SystemFileAssociations\.m2ts\shell\kinoz"; ValueType: string; ValueName: "Icon"; ValueData: "{app}\{#AppExe},0"
Root: HKA; Subkey: "Software\Classes\SystemFileAssociations\.m2ts\shell\kinoz\command"; ValueType: string; ValueData: """{app}\{#AppExe}"" ""%1"""
Root: HKA; Subkey: "Software\Classes\SystemFileAssociations\.3gp\shell\kinoz"; ValueType: string; ValueName: "MUIVerb"; ValueData: "Play with kinoz"; Flags: uninsdeletekey
Root: HKA; Subkey: "Software\Classes\SystemFileAssociations\.3gp\shell\kinoz"; ValueType: string; ValueName: "Icon"; ValueData: "{app}\{#AppExe},0"
Root: HKA; Subkey: "Software\Classes\SystemFileAssociations\.3gp\shell\kinoz\command"; ValueType: string; ValueData: """{app}\{#AppExe}"" ""%1"""
Root: HKA; Subkey: "Software\Classes\SystemFileAssociations\.mp3\shell\kinoz"; ValueType: string; ValueName: "MUIVerb"; ValueData: "Play with kinoz"; Flags: uninsdeletekey
Root: HKA; Subkey: "Software\Classes\SystemFileAssociations\.mp3\shell\kinoz"; ValueType: string; ValueName: "Icon"; ValueData: "{app}\{#AppExe},0"
Root: HKA; Subkey: "Software\Classes\SystemFileAssociations\.mp3\shell\kinoz\command"; ValueType: string; ValueData: """{app}\{#AppExe}"" ""%1"""
Root: HKA; Subkey: "Software\Classes\SystemFileAssociations\.flac\shell\kinoz"; ValueType: string; ValueName: "MUIVerb"; ValueData: "Play with kinoz"; Flags: uninsdeletekey
Root: HKA; Subkey: "Software\Classes\SystemFileAssociations\.flac\shell\kinoz"; ValueType: string; ValueName: "Icon"; ValueData: "{app}\{#AppExe},0"
Root: HKA; Subkey: "Software\Classes\SystemFileAssociations\.flac\shell\kinoz\command"; ValueType: string; ValueData: """{app}\{#AppExe}"" ""%1"""
Root: HKA; Subkey: "Software\Classes\SystemFileAssociations\.wav\shell\kinoz"; ValueType: string; ValueName: "MUIVerb"; ValueData: "Play with kinoz"; Flags: uninsdeletekey
Root: HKA; Subkey: "Software\Classes\SystemFileAssociations\.wav\shell\kinoz"; ValueType: string; ValueName: "Icon"; ValueData: "{app}\{#AppExe},0"
Root: HKA; Subkey: "Software\Classes\SystemFileAssociations\.wav\shell\kinoz\command"; ValueType: string; ValueData: """{app}\{#AppExe}"" ""%1"""
Root: HKA; Subkey: "Software\Classes\SystemFileAssociations\.ogg\shell\kinoz"; ValueType: string; ValueName: "MUIVerb"; ValueData: "Play with kinoz"; Flags: uninsdeletekey
Root: HKA; Subkey: "Software\Classes\SystemFileAssociations\.ogg\shell\kinoz"; ValueType: string; ValueName: "Icon"; ValueData: "{app}\{#AppExe},0"
Root: HKA; Subkey: "Software\Classes\SystemFileAssociations\.ogg\shell\kinoz\command"; ValueType: string; ValueData: """{app}\{#AppExe}"" ""%1"""
Root: HKA; Subkey: "Software\Classes\SystemFileAssociations\.opus\shell\kinoz"; ValueType: string; ValueName: "MUIVerb"; ValueData: "Play with kinoz"; Flags: uninsdeletekey
Root: HKA; Subkey: "Software\Classes\SystemFileAssociations\.opus\shell\kinoz"; ValueType: string; ValueName: "Icon"; ValueData: "{app}\{#AppExe},0"
Root: HKA; Subkey: "Software\Classes\SystemFileAssociations\.opus\shell\kinoz\command"; ValueType: string; ValueData: """{app}\{#AppExe}"" ""%1"""
Root: HKA; Subkey: "Software\Classes\SystemFileAssociations\.m4a\shell\kinoz"; ValueType: string; ValueName: "MUIVerb"; ValueData: "Play with kinoz"; Flags: uninsdeletekey
Root: HKA; Subkey: "Software\Classes\SystemFileAssociations\.m4a\shell\kinoz"; ValueType: string; ValueName: "Icon"; ValueData: "{app}\{#AppExe},0"
Root: HKA; Subkey: "Software\Classes\SystemFileAssociations\.m4a\shell\kinoz\command"; ValueType: string; ValueData: """{app}\{#AppExe}"" ""%1"""
Root: HKA; Subkey: "Software\Classes\SystemFileAssociations\.aac\shell\kinoz"; ValueType: string; ValueName: "MUIVerb"; ValueData: "Play with kinoz"; Flags: uninsdeletekey
Root: HKA; Subkey: "Software\Classes\SystemFileAssociations\.aac\shell\kinoz"; ValueType: string; ValueName: "Icon"; ValueData: "{app}\{#AppExe},0"
Root: HKA; Subkey: "Software\Classes\SystemFileAssociations\.aac\shell\kinoz\command"; ValueType: string; ValueData: """{app}\{#AppExe}"" ""%1"""
Root: HKA; Subkey: "Software\Classes\SystemFileAssociations\.wma\shell\kinoz"; ValueType: string; ValueName: "MUIVerb"; ValueData: "Play with kinoz"; Flags: uninsdeletekey
Root: HKA; Subkey: "Software\Classes\SystemFileAssociations\.wma\shell\kinoz"; ValueType: string; ValueName: "Icon"; ValueData: "{app}\{#AppExe},0"
Root: HKA; Subkey: "Software\Classes\SystemFileAssociations\.wma\shell\kinoz\command"; ValueType: string; ValueData: """{app}\{#AppExe}"" ""%1"""
Root: HKA; Subkey: "Software\Classes\SystemFileAssociations\.jpg\shell\kinoz"; ValueType: string; ValueName: "MUIVerb"; ValueData: "Play with kinoz"; Flags: uninsdeletekey
Root: HKA; Subkey: "Software\Classes\SystemFileAssociations\.jpg\shell\kinoz"; ValueType: string; ValueName: "Icon"; ValueData: "{app}\{#AppExe},0"
Root: HKA; Subkey: "Software\Classes\SystemFileAssociations\.jpg\shell\kinoz\command"; ValueType: string; ValueData: """{app}\{#AppExe}"" ""%1"""
Root: HKA; Subkey: "Software\Classes\SystemFileAssociations\.jpeg\shell\kinoz"; ValueType: string; ValueName: "MUIVerb"; ValueData: "Play with kinoz"; Flags: uninsdeletekey
Root: HKA; Subkey: "Software\Classes\SystemFileAssociations\.jpeg\shell\kinoz"; ValueType: string; ValueName: "Icon"; ValueData: "{app}\{#AppExe},0"
Root: HKA; Subkey: "Software\Classes\SystemFileAssociations\.jpeg\shell\kinoz\command"; ValueType: string; ValueData: """{app}\{#AppExe}"" ""%1"""
Root: HKA; Subkey: "Software\Classes\SystemFileAssociations\.png\shell\kinoz"; ValueType: string; ValueName: "MUIVerb"; ValueData: "Play with kinoz"; Flags: uninsdeletekey
Root: HKA; Subkey: "Software\Classes\SystemFileAssociations\.png\shell\kinoz"; ValueType: string; ValueName: "Icon"; ValueData: "{app}\{#AppExe},0"
Root: HKA; Subkey: "Software\Classes\SystemFileAssociations\.png\shell\kinoz\command"; ValueType: string; ValueData: """{app}\{#AppExe}"" ""%1"""
Root: HKA; Subkey: "Software\Classes\SystemFileAssociations\.gif\shell\kinoz"; ValueType: string; ValueName: "MUIVerb"; ValueData: "Play with kinoz"; Flags: uninsdeletekey
Root: HKA; Subkey: "Software\Classes\SystemFileAssociations\.gif\shell\kinoz"; ValueType: string; ValueName: "Icon"; ValueData: "{app}\{#AppExe},0"
Root: HKA; Subkey: "Software\Classes\SystemFileAssociations\.gif\shell\kinoz\command"; ValueType: string; ValueData: """{app}\{#AppExe}"" ""%1"""
Root: HKA; Subkey: "Software\Classes\SystemFileAssociations\.bmp\shell\kinoz"; ValueType: string; ValueName: "MUIVerb"; ValueData: "Play with kinoz"; Flags: uninsdeletekey
Root: HKA; Subkey: "Software\Classes\SystemFileAssociations\.bmp\shell\kinoz"; ValueType: string; ValueName: "Icon"; ValueData: "{app}\{#AppExe},0"
Root: HKA; Subkey: "Software\Classes\SystemFileAssociations\.bmp\shell\kinoz\command"; ValueType: string; ValueData: """{app}\{#AppExe}"" ""%1"""
Root: HKA; Subkey: "Software\Classes\SystemFileAssociations\.webp\shell\kinoz"; ValueType: string; ValueName: "MUIVerb"; ValueData: "Play with kinoz"; Flags: uninsdeletekey
Root: HKA; Subkey: "Software\Classes\SystemFileAssociations\.webp\shell\kinoz"; ValueType: string; ValueName: "Icon"; ValueData: "{app}\{#AppExe},0"
Root: HKA; Subkey: "Software\Classes\SystemFileAssociations\.webp\shell\kinoz\command"; ValueType: string; ValueData: """{app}\{#AppExe}"" ""%1"""
Root: HKA; Subkey: "Software\Classes\Directory\shell\kinoz"; ValueType: string; ValueName: "MUIVerb"; ValueData: "Open folder in kinoz"; Flags: uninsdeletekey
Root: HKA; Subkey: "Software\Classes\Directory\shell\kinoz"; ValueType: string; ValueName: "Icon"; ValueData: "{app}\{#AppExe},0"
Root: HKA; Subkey: "Software\Classes\Directory\shell\kinoz\command"; ValueType: string; ValueData: """{app}\{#AppExe}"" ""%1"""

[Run]
Filename: "{tmp}\vc_redist.x64.exe"; Parameters: "/install /quiet /norestart"; StatusMsg: "Installing the Visual C++ runtime..."; Check: NeedsVCRuntime; Flags: waituntilterminated
Filename: "{app}\{#AppExe}"; Description: "Open kinoz"; Flags: nowait postinstall skipifsilent

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

