#ifndef PackageDir
  #error PackageDir is required
#endif
#ifndef AppVersion
  #error AppVersion is required
#endif
#ifndef OutputDir
  #error OutputDir is required
#endif
#ifdef SmokeTest
  #define RegistryPrefix "Software\GPDFInstallerSmoke\"
  #define InstallerName "GPDF-Setup-Test"
#else
  #define RegistryPrefix ""
  #ifdef TestSigning
    #define InstallerName "GPDF-" + AppVersion + "-Setup-TestSigned"
  #else
    #define InstallerName "GPDF-" + AppVersion + "-Setup"
  #endif
#endif

[Setup]
AppId={{3D91A707-544B-49E7-AD30-75508CFCC9BB}
AppName=GPDF
AppVersion={#AppVersion}
AppVerName=GPDF {#AppVersion}
AppPublisher=Lautaro Grant
DefaultDirName={localappdata}\Programs\GPDF
DisableDirPage=no
DefaultGroupName=GPDF
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
OutputDir={#OutputDir}
OutputBaseFilename={#InstallerName}
SetupIconFile=..\resources\icons\gpdf.ico
UninstallDisplayIcon={app}\GPDF.exe,0
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
ShowLanguageDialog=yes
LanguageDetectionMethod=none
UsePreviousLanguage=no
ChangesAssociations=yes
CloseApplications=yes
RestartApplications=no
#ifdef TestSigning
SignTool=gpdf
SignedUninstaller=yes
#endif
#ifdef SmokeTest
CreateUninstallRegKey=no
#endif

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "spanish"; MessagesFile: "compiler:Languages\Spanish.isl"

[CustomMessages]
english.DesktopIcon=Create a desktop shortcut
spanish.DesktopIcon=Crear un acceso directo en el escritorio
english.OpenGpdf=Open GPDF
spanish.OpenGpdf=Abrir GPDF
english.PdfDocument=GPDF PDF document
spanish.PdfDocument=Documento PDF de GPDF
english.PdfDescription=PDF reader and annotation editor
spanish.PdfDescription=Lector y editor de anotaciones PDF
english.InitialLanguage=en
spanish.InitialLanguage=es

[Tasks]
Name: "desktopicon"; Description: "{cm:DesktopIcon}"; Flags: unchecked

[Files]
Source: "{#PackageDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

#ifndef SmokeTest
[Icons]
Name: "{autoprograms}\GPDF"; Filename: "{app}\GPDF.exe"; WorkingDir: "{app}"
Name: "{autodesktop}\GPDF"; Filename: "{app}\GPDF.exe"; WorkingDir: "{app}"; Tasks: desktopicon
#endif

[Registry]
; Advertise GPDF, leaving .pdf's default value and UserChoice untouched.
Root: HKCU; Subkey: "{#RegistryPrefix}Software\Classes\GPDF.PDF"; ValueType: string; ValueData: "{cm:PdfDocument}"; Flags: uninsdeletekey
Root: HKCU; Subkey: "{#RegistryPrefix}Software\Classes\GPDF.PDF\DefaultIcon"; ValueType: string; ValueData: """{app}\GPDF.exe"",1"
Root: HKCU; Subkey: "{#RegistryPrefix}Software\Classes\GPDF.PDF\shell\open\command"; ValueType: string; ValueData: """{app}\GPDF.exe"" ""%1"""
Root: HKCU; Subkey: "{#RegistryPrefix}Software\Classes\.pdf\OpenWithProgids"; ValueType: string; ValueName: "GPDF.PDF"; ValueData: ""; Flags: uninsdeletevalue uninsdeletekeyifempty
Root: HKCU; Subkey: "{#RegistryPrefix}Software\Classes\Applications\GPDF.exe"; ValueType: string; ValueName: "FriendlyAppName"; ValueData: "GPDF"; Flags: uninsdeletekey
Root: HKCU; Subkey: "{#RegistryPrefix}Software\Classes\Applications\GPDF.exe\DefaultIcon"; ValueType: string; ValueData: """{app}\GPDF.exe"",1"
Root: HKCU; Subkey: "{#RegistryPrefix}Software\Classes\Applications\GPDF.exe\shell\open\command"; ValueType: string; ValueData: """{app}\GPDF.exe"" ""%1"""
Root: HKCU; Subkey: "{#RegistryPrefix}Software\Classes\Applications\GPDF.exe\SupportedTypes"; ValueType: string; ValueName: ".pdf"; ValueData: ""
Root: HKCU; Subkey: "{#RegistryPrefix}Software\GPDF\Capabilities"; ValueType: string; ValueName: "ApplicationName"; ValueData: "GPDF"; Flags: uninsdeletekey
Root: HKCU; Subkey: "{#RegistryPrefix}Software\GPDF\Capabilities"; ValueType: string; ValueName: "ApplicationDescription"; ValueData: "{cm:PdfDescription}"
Root: HKCU; Subkey: "{#RegistryPrefix}Software\GPDF\Capabilities"; ValueType: string; ValueName: "ApplicationIcon"; ValueData: """{app}\GPDF.exe"",0"
Root: HKCU; Subkey: "{#RegistryPrefix}Software\GPDF\Capabilities\FileAssociations"; ValueType: string; ValueName: ".pdf"; ValueData: "GPDF.PDF"
Root: HKCU; Subkey: "{#RegistryPrefix}Software\RegisteredApplications"; ValueType: string; ValueName: "GPDF"; ValueData: "{#RegistryPrefix}Software\GPDF\Capabilities"; Flags: uninsdeletevalue

#ifndef SmokeTest
[Run]
; Initialize even when the optional launch is unchecked; existing preferences win.
Filename: "{app}\GPDF.exe"; Parameters: "--initial-language={cm:InitialLanguage} --initialize-language-only"; Flags: runhidden waituntilterminated
Filename: "{app}\GPDF.exe"; Description: "{cm:OpenGpdf}"; Flags: nowait postinstall skipifsilent
#endif
