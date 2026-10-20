; Darkride's Windows installer, built with Inno Setup 6 (https://jrsoftware.org/isinfo.php).
;
; package-windows.ps1 stages the files and runs this script; to run it by hand:
;   ISCC.exe /DAppVersion=2026.10.1 /DSourceDir=C:\stage\Darkride /DOutputDir=C:\out tools\installer\darkride.iss
;
; It replaces cmake/templates/NSIS.template.in for releases. That template is High Fidelity's, and needs NSIS plugins,
; the Sandbox and download servers that no longer exist.

#ifndef AppVersion
  #define AppVersion "0.0.0-dev"
#endif
#ifndef SourceDir
  #error Pass /DSourceDir=<folder with Darkride.exe and everything beside it>
#endif
#ifndef OutputDir
  #define OutputDir "."
#endif
#ifndef OutputBaseFilename
  #define OutputBaseFilename "Darkride-" + AppVersion + "-windows-x64-setup"
#endif
#define RepoDir AddBackslash(SourcePath) + "..\.."

[Setup]
; Never change AppId: it is how a newer installer finds and upgrades an older install.
AppId={{6A4F1D2E-8B1C-4E5A-9D47-0C3B2F8E7A61}
AppName=Darkride
AppVersion={#AppVersion}
AppVerName=Darkride {#AppVersion}
AppPublisher=Darkride
AppPublisherURL=https://vr.darkride.ai
AppSupportURL=https://github.com/caitlynmeeks/darkride-interface/issues
AppUpdatesURL=https://github.com/Madders/darkride-interface/releases
DefaultDirName={autopf}\Darkride
DisableProgramGroupPage=yes
; Installs for the current user without asking for admin rights; the first page offers an install for all users instead.
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
OutputDir={#OutputDir}
OutputBaseFilename={#OutputBaseFilename}
SetupIconFile={#RepoDir}\interface\icon\interface.ico
; made from the logo by the same script as interface\icon\interface.ico
WizardImageFile=wizard.bmp
WizardSmallImageFile=wizard-small.bmp
UninstallDisplayIcon={app}\Darkride.exe
UninstallDisplayName=Darkride
LicenseFile={#RepoDir}\LICENSE
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
CloseApplications=yes

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"
Name: "hifiurls"; Description: "Open hifi:// links with Darkride"; GroupDescription: "Links:"

[Files]
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[InstallDelete]
; An upgrade replaces the scripts and resources wholesale, so files removed from Darkride don't linger
Type: filesandordirs; Name: "{app}\scripts"
Type: filesandordirs; Name: "{app}\resources"
; the client was interface.exe until Caitlyn's main of 10-19 named it Darkride.exe
Type: files; Name: "{app}\interface.exe"

[Icons]
Name: "{autoprograms}\Darkride"; Filename: "{app}\Darkride.exe"
Name: "{autodesktop}\Darkride"; Filename: "{app}\Darkride.exe"; Tasks: desktopicon

[Registry]
; hifi:// links open a place in Darkride, as High Fidelity's installer set them up
Root: HKA; Subkey: "Software\Classes\hifi"; ValueType: string; ValueData: "URL:Darkride Protocol"; Flags: uninsdeletekey; Tasks: hifiurls
Root: HKA; Subkey: "Software\Classes\hifi"; ValueType: string; ValueName: "URL Protocol"; ValueData: ""; Tasks: hifiurls
Root: HKA; Subkey: "Software\Classes\hifi\DefaultIcon"; ValueType: string; ValueData: """{app}\Darkride.exe"",0"; Tasks: hifiurls
Root: HKA; Subkey: "Software\Classes\hifi\shell\open\command"; ValueType: string; ValueData: """{app}\Darkride.exe"" --protocol-url ""%1"""; Tasks: hifiurls

[Run]
Filename: "{app}\Darkride.exe"; Description: "{cm:LaunchProgram,Darkride}"; Flags: nowait postinstall skipifsilent
