;
; Copyright Flexible Software Solutions S.L. 2014
;

!include FileFunc.nsh
!include Sections.nsh
!include WinVer.nsh
!include x64.nsh

!define APPNAME "flexVDI guest agent"
!define WINLOGON_KEY "SOFTWARE\Microsoft\Windows NT\CurrentVersion\Winlogon"
!define UNINSTALL_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}"
!define REDMON_DLL "flexVDIprint_redmon.dll"
!define REDMON_NAME "flexVDI Redirection Port"
!define PORT_NAME "flexVDIprint"
!define PORT_CONFIG_KEY "SYSTEM\CurrentControlSet\Control\Print\Monitors\${REDMON_NAME}\Ports\${PORT_NAME}"

Name "${APPNAME} v@FLEXVDI_VERSION@"
OutFile "flexVDI_setup@WIN_BITS@_@FLEXVDI_VERSION@.exe"
InstallDir $PROGRAMFILES@WIN_BITS@\flexVDI
InstallDirRegKey HKLM "Software\${APPNAME}" "Install_Dir"
RequestExecutionLevel admin
SilentInstall silent
SilentUnInstall silent
SetOverwrite try

;Version Information
VIProductVersion "@FLEXVDI_VERSION@.0"
VIAddVersionKey "ProductName" "${APPNAME}"
VIAddVersionKey "CompanyName" "Flexible Software Solutions S.L."
VIAddVersionKey "LegalCopyright" "Copyright Flexible Software Solutions S.L. 2014"
VIAddVersionKey "FileDescription" "${APPNAME}"
VIAddVersionKey "FileVersion" "@FLEXVDI_VERSION@"
VIAddVersionKey "ProductVersion" "@FLEXVDI_VERSION@"


Section "flexVDI guest agent" install_section_id
    ; Set output path to the installation directory.
    SetOutPath $INSTDIR
    SetOverwrite try

    ; Write the installation path into the registry
    WriteRegStr HKLM "SOFTWARE\${APPNAME}" "Install_Dir" "$INSTDIR"

    ; Write the uninstall keys for Windows
    WriteRegStr HKLM "${UNINSTALL_KEY}"   "DisplayName" "${APPNAME}"
    WriteRegStr HKLM "${UNINSTALL_KEY}"   "DisplayIcon" "$INSTDIR\flexvdi-guest-agent.exe"
    WriteRegStr HKLM "${UNINSTALL_KEY}"   "DisplayVersion" "@FLEXVDI_VERSION@"
    WriteRegStr HKLM "${UNINSTALL_KEY}"   "InstallLocation" "$INSTDIR"
    WriteRegStr HKLM "${UNINSTALL_KEY}"   "Publisher" "Flexible Software Solutions S.L."
    WriteRegStr HKLM "${UNINSTALL_KEY}"   "UninstallString" '"$INSTDIR\uninstall.exe"'
    WriteRegDWORD HKLM "${UNINSTALL_KEY}" "NoModify" 1
    WriteRegDWORD HKLM "${UNINSTALL_KEY}" "NoRepair" 1
    WriteUninstaller "uninstall.exe"

    ; Check architecture
    ${If} ${RunningX64}
    ${AndIf} @WIN_BITS@ = 32
        MessageBox MB_OK "Use the 64bit installer on x68_64 architecture"
        Quit
    ${ElseIf} !${RunningX64}
    ${AndIf} @WIN_BITS@ = 64
        MessageBox MB_OK "Use the 32bit installer on i686 architecture"
        Quit
    ${EndIf}

    ; Stop flexVDI service, ignore if it does not exist
    nsExec::Exec 'sc stop flexvdi_service'
    nsExec::Exec '"$INSTDIR\flexvdi-guest-agent.exe" uninstall'

    ; Agent
    File "flexvdi-guest-agent.exe"

    ; start flexVDI service
    nsExec::Exec '"$INSTDIR\flexvdi-guest-agent.exe" install'
    nsExec::Exec 'sc start flexvdi_service'
    ; TODO: Check errors installing the service

    ; SSO
    ${If} ${IsWinXP}
        File "cred-connectors/gina/flexVDIGina.dll"
        Rename /REBOOTOK $INSTDIR\flexVDIGina.dll $SYSDIR\flexVDIGina.dll
        ReadRegStr $0 HKLM "${UNINSTALL_KEY}" "OldGinaDLL"
        IfErrors 0 oldgina_present
            ReadRegStr $0 HKLM "${WINLOGON_KEY}" "GinaDLL"
            WriteRegStr HKLM "${UNINSTALL_KEY}" "OldGinaDLL" "$0"
        oldgina_present:
        WriteRegStr HKLM "${WINLOGON_KEY}" "GinaDLL" "flexVDIGina.dll"
    ${Else}
        MessageBox MB_OK "Installed credential provider"
    ${EndIf}

    ; Print driver
    File "print/windriver/setredmon.exe" "print/windriver/unredmon.exe" "print/windriver/redmon.dll"
    Rename /REBOOTOK "$INSTDIR\redmon.dll" "$SYSDIR\${REDMON_DLL}"
    nsExec::ExecToStack '"$INSTDIR\setredmon.exe" "${REDMON_NAME}" "${REDMON_DLL}" "${PORT_NAME}"'
    Pop $0
    ${If} $0 != 0
        Pop $1
        MessageBox MB_OK "setredmon returned $0 $1"
    ${EndIf}
    Delete "$INSTDIR\setredmon.exe"
    SetOutPath "$INSTDIR\printdriver"
    File "@PROJECT_SOURCE_DIR@/print/windriver/flexvdips.*"
    File "@PROJECT_SOURCE_DIR@/print/windriver/gs9.15/gsdll@WIN_BITS@.dll"
    File "print/windriver/filter.exe"
    nsExec::Exec 'rundll32 printui.dll,PrintUIEntry /if /n "flexVDI Printer" /f "$INSTDIR\printdriver\flexvdips.inf" /m "flexVDI Printer" /r "${PORT_NAME}"'
    WriteRegStr HKLM "${PORT_CONFIG_KEY}"   "Command" '$INSTDIR\printdriver\filter.exe'
    WriteRegStr HKLM "${PORT_CONFIG_KEY}"   "Arguments" ''
    WriteRegDWORD HKLM "${PORT_CONFIG_KEY}" "RunUser" 0

    ; Compute installed size
    ${GetSize} "$INSTDIR" "/S=0K" $0 $1 $2
    IntFmt $0 "0x%08X" $0
    WriteRegDWORD HKLM "${UNINSTALL_KEY}" "EstimatedSize" "$0"

    IfRebootFlag 0 noreboot
        MessageBox MB_YESNO "System needs to be rebooted, reboot now?" IDNO noreboot
            Reboot
    noreboot:
SectionEnd

Function .onInit
    SectionSetFlags ${install_section_id} ${SF_SELECTED}
    ${IfNot} ${AtLeastWinXP}
        MessageBox MB_OK "XP and above required"
        Quit
    ${EndIf}
FunctionEnd


Section "Uninstall"
    nsExec::Exec 'rundll32 printui.dll,PrintUIEntry /dl /n "flexVDI Printer"'
    nsExec::Exec 'rundll32 printui.dll,PrintUIEntry /dd /m "flexVDI Printer"'
    nsExec::Exec '"$INSTDIR\unredmon.exe" "${REDMON_NAME}"'
    Pop $0
    ${If} $0 = 0
        Delete /REBOOTOK "$SYSDIR\redmon.dll"
    ${EndIf}

    ${If} ${IsWinXP}
        ReadRegStr $0 HKLM "${UNINSTALL_KEY}" "OldGinaDLL"
        WriteRegStr HKLM "${WINLOGON_KEY}" "GinaDLL" "$0"
        Delete /REBOOTOK $SYSDIR\flexVDIGina.dll
    ${Else}
        MessageBox MB_OK "Removed credential provider"
    ${EndIf}

    DeleteRegKey HKLM "${UNINSTALL_KEY}"
    DeleteRegKey HKLM "SOFTWARE\${APPNAME}"
    nsExec::Exec 'sc stop flexvdi_service'
    nsExec::Exec '"$INSTDIR\flexvdi-guest-agent.exe" uninstall'
    RMDir /r /REBOOTOK "$INSTDIR"

    IfRebootFlag 0 noreboot
        MessageBox MB_YESNO "System needs to be rebooted, reboot now?" IDNO noreboot
            Reboot
    noreboot:
SectionEnd
