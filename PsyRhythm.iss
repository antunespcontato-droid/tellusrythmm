; Inno Setup – instalador do VST3 (Windows). Compilar: iscc installer\PsyRhythm.iss
; Coloque assets\logo.ico (ícone gerado da SUA logo) e assets\logo.bmp (até ~164x314 para o painel lateral) para usar a logo oficial.
#define AppName "PsyRhythm"
#define AppVersion "0.1.0"
#ifndef VST3Source
  #define VST3Source "..\build\PsyRhythm_artefacts\Release\VST3\PsyRhythm.vst3"
#endif

[Setup]
AppId={{B7E1F7C2-5B7A-4C59-9C1E-5A8F0D6B1A11}
AppName={#AppName}
AppVersion={#AppVersion}
DefaultDirName={commoncf64}\VST3
DisableDirPage=no
DirExistsWarning=no
UsePreviousAppDir=no
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes
PrivilegesRequired=admin
ArchitecturesInstallIn64BitMode=x64compatible
ArchitecturesAllowed=x64compatible
OutputDir=..\dist
OutputBaseFilename={#AppName}-Setup-{#AppVersion}
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
#ifexist "..\assets\logo.ico"
SetupIconFile=..\assets\logo.ico
#endif
#ifexist "..\assets\logo.bmp"
WizardSmallImageFile=..\assets\logo.bmp
WizardImageFile=..\assets\logo.bmp
#endif
UninstallFilesDir={autopf}\{#AppName}

[Messages]
SelectDirLabel3=O plugin VST3 será instalado na pasta abaixo. O padrão é a pasta VST3 comum do Windows; escolha outra se o seu Ableton Live usa uma pasta personalizada.

[Files]
Source: "{#VST3Source}\*"; DestDir: "{app}\{#AppName}.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs

[Code]
function InitializeSetup(): Boolean;
begin
  Result := True;
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssPostInstall then
  begin
    if not FileExists(ExpandConstant('{app}\{#AppName}.vst3\Contents\x86_64-win\{#AppName}.vst3')) then
      MsgBox('Erro: o arquivo do plugin não foi encontrado após a instalação em ' + ExpandConstant('{app}') + '. Execute o instalador como administrador e tente novamente.', mbError, MB_OK)
    else
      MsgBox('Instalação concluída e verificada.' + #13#10 + 'Plugin em: ' + ExpandConstant('{app}\{#AppName}.vst3') + #13#10#13#10 + 'No Ableton Live: Preferences > Plug-ins > ative VST3 e clique em Rescan.', mbInformation, MB_OK);
  end;
end;
