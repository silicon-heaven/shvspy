#define APP_NAME "shvspy"
#define APP_NAME_LOWER "shvspy"

[Setup]
AppName={#APP_NAME}
AppVerName={#APP_NAME} {#VERSION}
AppPublisher=Elektroline
AppPublisherURL=http://www.elektroline.cz
DefaultDirName=C:\{#APP_NAME}
DefaultGroupName={#APP_NAME}
OutputDir={#BUILD_DIR}\_inno\{#APP_NAME_LOWER}
OutputBaseFilename={#APP_NAME_LOWER}-{#VERSION}-setup
SetupIconFile=.\{#APP_NAME_LOWER}.ico
Compression=lzma
SolidCompression=yes
ChangesAssociations = yes

[Tasks]
Name: desktopicon; Description: {cm:CreateDesktopIcon}; GroupDescription: {cm:AdditionalIcons}
Name: quicklaunchicon; Description: {cm:CreateQuickLaunchIcon}; GroupDescription: {cm:AdditionalIcons}; Flags: unchecked

[Files]
Source: {#BUILD_DIR}\bin\*; DestDir: {app}; Flags: ignoreversion recursesubdirs


[Icons]
Name: {group}\{#APP_NAME}; Filename: {app}\{#APP_NAME_LOWER}.exe
Name: {group}\{cm:UninstallProgram,{#APP_NAME}}; Filename: {uninstallexe}
Name: {userdesktop}\{#APP_NAME}; Filename: {app}\{#APP_NAME_LOWER}.exe; Tasks: desktopicon
Name: {userappdata}\Microsoft\Internet Explorer\Quick Launch\{#APP_NAME}; Filename: {app}\{#APP_NAME_LOWER}.exe; Tasks: quicklaunchicon

[Run]
Filename: {app}\{#APP_NAME_LOWER}.exe; Description: {cm:LaunchProgram,{#APP_NAME}}; Flags: nowait postinstall
