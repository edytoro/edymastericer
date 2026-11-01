; Instalador de EdyMastericer (Inno Setup 6)
; Instala el plugin VST3 de 32 bits y, en Windows de 64 bits, tambien el de 64 bits,
; cada uno en la carpeta VST3 que le corresponde.

[Setup]
AppName=EdyMastericer
AppVersion=1.0.0
AppPublisher=Edy
DefaultDirName={autopf}\EdyMastericer
DisableDirPage=yes
DisableProgramGroupPage=yes
OutputDir=Output
OutputBaseFilename=EdyMastericer_Setup
Compression=lzma
SolidCompression=yes
PrivilegesRequired=admin
ArchitecturesInstallIn64BitMode=x64
WizardStyle=modern

[Languages]
Name: "spanish"; MessagesFile: "compiler:Languages\Spanish.isl"

[Files]
; 32 bits (Cubase de 32 bits, carpeta "Archivos de programa (x86)")
Source: "bundle\win32\*"; DestDir: "{commoncf32}\VST3\EdyMastericer.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs
; 64 bits (Cubase de 64 bits)
Source: "bundle\x64\*"; DestDir: "{commoncf64}\VST3\EdyMastericer.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs; Check: Is64BitInstallMode
