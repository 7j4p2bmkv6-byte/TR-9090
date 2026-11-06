[Setup]
AppName=TR-9090
AppVersion=1.0.0
AppPublisher=Landgren Audio
DefaultDirName={autopf}\Landgren Audio\TR-9090
DefaultGroupName=TR-9090
OutputDir=Output
OutputBaseFilename=TR-9090-Setup
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
Compression=lzma2
SolidCompression=yes

[Tasks]
Name: "desktopicon"; Description: "Create a desktop icon"; Flags: unchecked

[Files]
Source: "..\build\TR9090_artefacts\Release\Standalone\TR-9090.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\build\TR9090_artefacts\Release\VST3\TR-9090.vst3\*"; DestDir: "{commoncf64}\VST3\TR-9090.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\TR-9090"; Filename: "{app}\TR-9090.exe"
Name: "{group}\Uninstall TR-9090"; Filename: "{uninstallexe}"
Name: "{autodesktop}\TR-9090"; Filename: "{app}\TR-9090.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\TR-9090.exe"; Description: "Launch TR-9090"; Flags: nowait postinstall skipifsilent
