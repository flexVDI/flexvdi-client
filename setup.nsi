!include FileFunc.nsh
!include Sections.nsh
!include WinVer.nsh

!define APPNAME "flexVDI guest agent"
!define WINLOGON_KEY "SOFTWARE\Microsoft\Windows NT\CurrentVersion\Winlogon"
!define ARP "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}"

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

    ; Stop flexVDI service
    nsExec::Exec 'sc stop flexvdi_service'
    nsExec::Exec '"$INSTDIR\flexvdi-guest-agent.exe" uninstall'

    ; Put file there
    File "flexvdi-guest-agent.exe"

    ; start flexVDI service
    nsExec::Exec '"$INSTDIR\flexvdi-guest-agent.exe" install'
    nsExec::Exec 'sc start flexvdi_service'

    ${If} ${IsWinXP}
        File "cred-connectors/gina/flexVDIGina.dll"
        Rename /REBOOTOK $INSTDIR\flexVDIGina.dll $SYSDIR\flexVDIGina.dll
        WriteRegStr HKLM "${WINLOGON_KEY}" "GinaDLL" "flexVDIGina.dll"
    ${Else}
        MessageBox MB_OK "Installed credential provider"
    ${EndIf}

    ; Write the installation path into the registry
    WriteRegStr HKLM "SOFTWARE\${APPNAME}" "Install_Dir" "$INSTDIR"

    ; Write the uninstall keys for Windows
    WriteRegStr HKLM "${ARP}"   "DisplayName" "${APPNAME}"
;    WriteRegStr HKLM "${ARP}"   "DisplayIcon" "$INSTDIR\hdrmerge.exe"
    WriteRegStr HKLM "${ARP}"   "DisplayVersion" "@FLEXVDI_VERSION@"
    WriteRegStr HKLM "${ARP}"   "InstallLocation" "$INSTDIR"
    ; Compute installed size
    ${GetSize} "$INSTDIR" "/S=0K" $0 $1 $2
    IntFmt $0 "0x%08X" $0
    WriteRegDWORD HKLM "${ARP}" "EstimatedSize" "$0"
    WriteRegStr HKLM "${ARP}"   "Publisher" "Flexible Software Solutions S.L."
    WriteRegStr HKLM "${ARP}"   "UninstallString" '"$INSTDIR\uninstall.exe"'
    WriteRegDWORD HKLM "${ARP}" "NoModify" 1
    WriteRegDWORD HKLM "${ARP}" "NoRepair" 1
    WriteUninstaller "uninstall.exe"

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
    DeleteRegKey HKLM "${ARP}"
    DeleteRegKey HKLM "SOFTWARE\${APPNAME}"
    nsExec::Exec 'sc stop flexvdi_service'
    nsExec::Exec '"$INSTDIR\flexvdi-guest-agent.exe" uninstall'
    WriteRegStr HKLM "${WINLOGON_KEY}" "GinaDLL" "msgina.dll"
    RMDir /r /REBOOTOK $INSTDIR
    Delete /REBOOTOK $SYSDIR\flexVDIGina.dll
    IfRebootFlag 0 noreboot
        MessageBox MB_YESNO "System needs to be rebooted, reboot now?" IDNO noreboot
            Reboot
    noreboot:
SectionEnd
