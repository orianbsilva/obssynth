; The Windows installer of the Obs instruments: the plugins where hosts look for them, the standalone programs with
; their Start menu entries, an uninstaller. Built by release.yml (Inno Setup 7):
;   ISCC /DAppVersion=v0.1.0 /DVst3Dir=... /DClapDir=... /DAppsDir=... /DOutDir=... ObsInstruments.iss
;   VST3  C:\Program Files\Common Files\VST3\<Name>.vst3        (each with its editor and wavetables inside)
;   CLAP  C:\Program Files\Common Files\CLAP\Obs Instruments\   (the .clap files, their editors, pcm\)
;   apps  C:\Program Files\Obs Instruments\                     (obssynthct1.exe, obssynth4d.exe, ...)
#ifndef AppVersion
  #define AppVersion "dev"
#endif
#ifndef OutDir
  #define OutDir "."
#endif

[Setup]
AppId={{83BB92AE-46F6-5BE4-AE43-A231926A2308}
AppName=Obs Instruments
AppVersion={#AppVersion}
AppVerName=Obs Instruments {#AppVersion}
AppPublisher=Carbonian
AppPublisherURL=https://github.com/orianbsilva/obssynth
AppSupportURL=https://github.com/orianbsilva/obssynth/releases
DefaultDirName={autopf}\Obs Instruments
DefaultGroupName=Obs Instruments
DisableProgramGroupPage=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
PrivilegesRequired=admin
OutputDir={#OutDir}
OutputBaseFilename=ObsInstruments-{#AppVersion}-windows-x64-setup
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
UninstallDisplayName=Obs Instruments {#AppVersion}
CloseApplications=yes

[Types]
Name: "full"; Description: "Plugins (VST3 and CLAP) and the standalone programs"
Name: "plugins"; Description: "The plugins only (VST3 and CLAP)"
Name: "custom"; Description: "Choose"; Flags: iscustom

[Components]
Name: "vst3"; Description: "VST3 plugins (Common Files\VST3: Ableton Live, Cubase, FL Studio, ...)"; Types: full plugins
Name: "clap"; Description: "CLAP plugins (Common Files\CLAP: Bitwig, REAPER, ...)"; Types: full plugins
Name: "apps"; Description: "Standalone programs, in the Start menu"; Types: full

[Files]
Source: "{#Vst3Dir}\*"; DestDir: "{commoncf64}\VST3"; Components: vst3; Flags: recursesubdirs createallsubdirs ignoreversion
Source: "{#ClapDir}\*"; DestDir: "{commoncf64}\CLAP\Obs Instruments"; Components: clap; Flags: recursesubdirs createallsubdirs ignoreversion
Source: "{#AppsDir}\*"; DestDir: "{app}"; Components: apps; Flags: recursesubdirs createallsubdirs ignoreversion

[Icons]
Name: "{group}\ObsSynth CT1"; Filename: "{app}\obssynthct1.exe"; WorkingDir: "{app}"; Components: apps
Name: "{group}\ObsSynth 4D"; Filename: "{app}\obssynth4d.exe"; WorkingDir: "{app}"; Components: apps
Name: "{group}\ObsSynth One"; Filename: "{app}\obssynthone.exe"; WorkingDir: "{app}"; Components: apps
Name: "{group}\ObsDrums"; Filename: "{app}\obsdrums.exe"; WorkingDir: "{app}"; Components: apps
Name: "{group}\Uninstall Obs Instruments"; Filename: "{uninstallexe}"

[Messages]
FinishedLabel=The Obs instruments are installed.%n%nIn your DAW, rescan the plugins: they are under "Carbonian". In Ableton Live, Preferences > Plug-ins > "Use VST3 Plug-in System Folders" on, then Rescan.
