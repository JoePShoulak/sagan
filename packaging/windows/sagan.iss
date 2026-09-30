#ifndef SourceDir
  #define SourceDir "..\..\build\windows-stage"
#endif
#ifndef AppVersion
  #define AppVersion "0.0.0"
#endif
#ifdef SignToolName
  #define SaganBinarySignFlag " signonce"
#else
  #define SaganBinarySignFlag ""
#endif

[Setup]
AppId={{09D785A5-B94E-4D75-9CC9-E57831637DD5}
AppName=Sagan
AppVersion={#AppVersion}
AppPublisher=Joe P. Shoulak
AppPublisherURL=https://github.com/JoePShoulak/sagan
AppSupportURL=https://github.com/JoePShoulak/sagan/issues
AppUpdatesURL=https://github.com/JoePShoulak/sagan/releases
DefaultDirName={autopf}\Sagan
DefaultGroupName=Sagan
DisableProgramGroupPage=yes
LicenseFile={#SourceDir}\licenses\Sagan-GPL-3.0.txt
SetupIconFile={#SourceDir}\assets\sagan.ico
UninstallDisplayIcon={app}\assets\sagan.ico
OutputBaseFilename=sagan-{#AppVersion}-windows-x64
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64os
ArchitecturesInstallIn64BitMode=x64os
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
ChangesAssociations=yes
ChangesEnvironment=yes
MinVersion=10.0
#ifdef SignToolName
SignTool={#SignToolName}
SignedUninstaller=yes
#endif

[Tasks]
Name: addtopath; Description: "Add Sagan to PATH"; GroupDescription: "Command-line integration:"; Flags: checkedonce
Name: fileassociation; Description: "Run .sagan files from File Explorer"; GroupDescription: "File integration:"; Flags: checkedonce

[Files]
Source: "{#SourceDir}\bin\*"; DestDir: "{app}\bin"; Flags: ignoreversion{#SaganBinarySignFlag}
Source: "{#SourceDir}\toolchain\*"; DestDir: "{app}\toolchain"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#SourceDir}\assets\*"; DestDir: "{app}\assets"; Flags: ignoreversion
Source: "{#SourceDir}\licenses\*"; DestDir: "{app}\licenses"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#SourceDir}\VERSION"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\Sagan Command Prompt"; Filename: "{cmd}"; Parameters: "/K sagan --version"; WorkingDir: "{userdocs}"
Name: "{group}\Uninstall Sagan"; Filename: "{uninstallexe}"

[Registry]
Root: HKA; Subkey: "Software\Classes\.sagan"; ValueType: string; ValueName: ""; ValueData: "Sagan.Source"; Tasks: fileassociation; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\Classes\Sagan.Source"; ValueType: string; ValueName: ""; ValueData: "Sagan source file"; Tasks: fileassociation; Flags: uninsdeletekey
Root: HKA; Subkey: "Software\Classes\Sagan.Source\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\assets\sagan.ico"; Tasks: fileassociation
Root: HKA; Subkey: "Software\Classes\Sagan.Source\shell"; ValueType: string; ValueName: ""; ValueData: "run"; Tasks: fileassociation
Root: HKA; Subkey: "Software\Classes\Sagan.Source\shell\run"; ValueType: string; ValueName: ""; ValueData: "Run Sagan Program"; Tasks: fileassociation
Root: HKA; Subkey: "Software\Classes\Sagan.Source\shell\run\command"; ValueType: string; ValueName: ""; ValueData: """{app}\bin\sagan-launch.exe"" ""%1"""; Tasks: fileassociation
Root: HKA; Subkey: "Software\Classes\Sagan.Source\shell\console"; ValueType: string; ValueName: ""; ValueData: "Run in Terminal"; Tasks: fileassociation
Root: HKA; Subkey: "Software\Classes\Sagan.Source\shell\console\command"; ValueType: string; ValueName: ""; ValueData: """{app}\bin\sagan-launch.exe"" --console ""%1"""; Tasks: fileassociation
Root: HKA; Subkey: "Software\Classes\Sagan.Source\shell\windowed"; ValueType: string; ValueName: ""; ValueData: "Run Without Terminal"; Tasks: fileassociation
Root: HKA; Subkey: "Software\Classes\Sagan.Source\shell\windowed\command"; ValueType: string; ValueName: ""; ValueData: """{app}\bin\sagan-launch.exe"" --windowed ""%1"""; Tasks: fileassociation
Root: HKA; Subkey: "Software\Classes\Sagan.Source\shell\edit"; ValueType: string; ValueName: ""; ValueData: "Choose Editor..."; Tasks: fileassociation
Root: HKA; Subkey: "Software\Classes\Sagan.Source\shell\edit\command"; ValueType: string; ValueName: ""; ValueData: "rundll32.exe shell32.dll,OpenAs_RunDLL ""%1"""; Tasks: fileassociation

[Code]
const
  PathValue = 'Path';
  UninstallKey = 'Software\Microsoft\Windows\CurrentVersion\Uninstall\{09D785A5-B94E-4D75-9CC9-E57831637DD5}_is1';

function VersionPart(Version: String; Part: Integer): Integer;
var
  Index: Integer;
  Separator: Integer;
  Current: String;
begin
  Result := 0;
  Current := Version;
  for Index := 0 to Part do
  begin
    Separator := Pos('.', Current);
    if Separator = 0 then
    begin
      if Index = Part then Result := StrToIntDef(Current, 0);
      Exit;
    end;
    if Index = Part then
    begin
      Result := StrToIntDef(Copy(Current, 1, Separator - 1), 0);
      Exit;
    end;
    Delete(Current, 1, Separator);
  end;
end;

function CompareSemanticVersions(Left: String; Right: String): Integer;
var
  Part: Integer;
  LeftPart: Integer;
  RightPart: Integer;
begin
  Result := 0;
  for Part := 0 to 2 do
  begin
    LeftPart := VersionPart(Left, Part);
    RightPart := VersionPart(Right, Part);
    if LeftPart < RightPart then
    begin
      Result := -1;
      Exit;
    end;
    if LeftPart > RightPart then
    begin
      Result := 1;
      Exit;
    end;
  end;
end;

function InstalledVersion(var Version: String): Boolean;
begin
  Result := RegQueryStringValue(HKCU, UninstallKey, 'DisplayVersion', Version);
  if not Result then
    Result := RegQueryStringValue(HKLM, UninstallKey, 'DisplayVersion', Version);
end;

function InitializeSetup(): Boolean;
var
  ExistingVersion: String;
begin
  Result := True;
  if InstalledVersion(ExistingVersion) and
     (CompareSemanticVersions(ExistingVersion, '{#AppVersion}') > 0) then
  begin
    SuppressibleMsgBox(
      'Sagan ' + ExistingVersion + ' is already installed. Uninstall it before installing the older ' +
      '{#AppVersion} release.', mbCriticalError, MB_OK, IDOK);
    Result := False;
  end;
end;

procedure GetEnvironmentLocation(var RootKey: Integer; var EnvironmentKey: String);
begin
  if IsAdminInstallMode then
  begin
    RootKey := HKLM;
    EnvironmentKey := 'SYSTEM\CurrentControlSet\Control\Session Manager\Environment';
  end
  else
  begin
    RootKey := HKCU;
    EnvironmentKey := 'Environment';
  end;
end;

function NormalizePath(Value: String): String;
begin
  Result := RemoveBackslashUnlessRoot(ExpandFileName(Value));
end;

function PathContains(PathValueData: String; Directory: String): Boolean;
var
  Remaining: String;
  Item: String;
  Separator: Integer;
begin
  Result := False;
  Remaining := PathValueData;
  while Remaining <> '' do
  begin
    Separator := Pos(';', Remaining);
    if Separator = 0 then
    begin
      Item := Remaining;
      Remaining := '';
    end
    else
    begin
      Item := Copy(Remaining, 1, Separator - 1);
      Delete(Remaining, 1, Separator);
    end;
    if (Item <> '') and (CompareText(NormalizePath(Item), NormalizePath(Directory)) = 0) then
    begin
      Result := True;
      Exit;
    end;
  end;
end;

procedure AddToUserPath;
var
  Existing: String;
  Directory: String;
  EnvironmentKey: String;
  RootKey: Integer;
begin
  if not WizardIsTaskSelected('addtopath') then Exit;
  GetEnvironmentLocation(RootKey, EnvironmentKey);
  Directory := ExpandConstant('{app}\bin');
  RegQueryStringValue(RootKey, EnvironmentKey, PathValue, Existing);
  if not PathContains(Existing, Directory) then
  begin
    if (Existing <> '') and (Existing[Length(Existing)] <> ';') then Existing := Existing + ';';
    RegWriteExpandStringValue(RootKey, EnvironmentKey, PathValue, Existing + Directory);
  end;
end;

procedure RemoveFromUserPath;
var
  Existing: String;
  Directory: String;
  Remaining: String;
  Updated: String;
  Item: String;
  Separator: Integer;
  EnvironmentKey: String;
  RootKey: Integer;
begin
  GetEnvironmentLocation(RootKey, EnvironmentKey);
  Directory := ExpandConstant('{app}\bin');
  if not RegQueryStringValue(RootKey, EnvironmentKey, PathValue, Existing) then Exit;
  Remaining := Existing;
  Updated := '';
  while Remaining <> '' do
  begin
    Separator := Pos(';', Remaining);
    if Separator = 0 then
    begin
      Item := Remaining;
      Remaining := '';
    end
    else
    begin
      Item := Copy(Remaining, 1, Separator - 1);
      Delete(Remaining, 1, Separator);
    end;
    if (Item <> '') and (CompareText(NormalizePath(Item), NormalizePath(Directory)) <> 0) then
    begin
      if Updated <> '' then Updated := Updated + ';';
      Updated := Updated + Item;
    end;
  end;
  RegWriteExpandStringValue(RootKey, EnvironmentKey, PathValue, Updated);
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssPostInstall then
  begin
    AddToUserPath;
  end;
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
  if CurUninstallStep = usUninstall then
  begin
    RemoveFromUserPath;
  end;
end;
