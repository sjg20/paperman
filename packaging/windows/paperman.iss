; Installer for paperman on Windows, built with Inno Setup by
; scripts/win-installer.sh, or 'make -f Makefile.win installer', which
; runs:
;
;    iscc /DStageDir=..\..\dist\paperman /DAppVersion=1.3.1 paperman.iss
;
; StageDir is what scripts/win-stage.sh gathered: the program and every
; library it needs. The installer asks for no more rights than the user
; already has, so it lands in the user's own programs folder and needs no
; administrator; someone who wants it for everyone on the machine can say
; so when it starts, and it goes to Program Files instead.

#ifndef AppVersion
  #define AppVersion "0.0.0"
#endif
#ifndef StageDir
  #define StageDir "..\..\dist\paperman"
#endif

[Setup]
AppName=Paperman
AppVersion={#AppVersion}
AppPublisher=Simon Glass
AppPublisherURL=https://github.com/sjg20/paperman
DefaultDirName={autopf}\Paperman
DefaultGroupName=Paperman
UninstallDisplayIcon={app}\bin\paperman.exe
OutputBaseFilename=paperman-setup-{#AppVersion}
OutputDir=.
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
DisableProgramGroupPage=yes
LicenseFile=..\..\debian\copyright

[Tasks]
Name: desktopicon; Description: "Put a shortcut on the desktop"; \
   GroupDescription: "Shortcuts:"
Name: associate; Description: "Open .max files with Paperman"; \
   GroupDescription: "Files:"

[Files]
Source: "{#StageDir}\*"; DestDir: "{app}"; Flags: recursesubdirs createallsubdirs ignoreversion

[Icons]
Name: "{group}\Paperman"; Filename: "{app}\bin\paperman.exe"
Name: "{group}\Uninstall Paperman"; Filename: "{uninstallexe}"
Name: "{autodesktop}\Paperman"; Filename: "{app}\bin\paperman.exe"; Tasks: desktopicon

[Registry]
; a stack of scanned pages, which is what paperman keeps its pages in
Root: HKA; Subkey: "Software\Classes\.max"; ValueType: string; \
   ValueName: ""; ValueData: "Paperman.Stack"; \
   Flags: uninsdeletevalue; Tasks: associate
Root: HKA; Subkey: "Software\Classes\Paperman.Stack"; ValueType: string; \
   ValueName: ""; ValueData: "Paperman stack"; \
   Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\Classes\Paperman.Stack\DefaultIcon"; \
   ValueType: string; ValueName: ""; ValueData: "{app}\bin\paperman.exe,0"; \
   Tasks: associate
Root: HKA; Subkey: "Software\Classes\Paperman.Stack\shell\open\command"; \
   ValueType: string; ValueName: ""; \
   ValueData: """{app}\bin\paperman.exe"" ""%1"""; Tasks: associate

[Run]
Filename: "{app}\bin\paperman.exe"; Description: "Start Paperman"; \
   Flags: nowait postinstall skipifsilent
