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
!define PDF_PRINTER_NAME "flexVDI PDF Printer"

Name "${APPNAME} v@FLEXVDI_VERSION@"
OutFile "flexVDI_setup@WIN_BITS@_@FLEXVDI_VERSION@.exe"
InstallDir $PROGRAMFILES@WIN_BITS@\flexVDI
InstallDirRegKey HKLM "Software\${APPNAME}" "Install_Dir"
RequestExecutionLevel admin
ShowInstDetails show
ShowUninstDetails show
SetOverwrite try
SetPluginUnload  alwaysoff

;Version Information
VIProductVersion "@FLEXVDI_VERSION@.0"
VIAddVersionKey "ProductName" "${APPNAME}"
VIAddVersionKey "CompanyName" "Flexible Software Solutions S.L."
VIAddVersionKey "LegalCopyright" "Copyright Flexible Software Solutions S.L. 2014"
VIAddVersionKey "FileDescription" "${APPNAME}"
VIAddVersionKey "FileVersion" "@FLEXVDI_VERSION@"
VIAddVersionKey "ProductVersion" "@FLEXVDI_VERSION@"


Section "flexVDI guest agent" install_section_id
    ${DisableX64FSRedirection}
    ; Set output path to the installation directory.
    SetOutPath $INSTDIR
    SetAutoClose true

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
        ${If} @WIN_BITS@ = 32
            MessageBox MB_OK "Use the 64bit installer on x68_64 architecture"
            Quit
        ${EndIf}
    ${Else}
        ${If} @WIN_BITS@ = 64
            MessageBox MB_OK "Use the 32bit installer on i686 architecture"
            Quit
        ${EndIf}
    ${EndIf}

    ; Agent
    nsExec::Exec '"$INSTDIR\flexvdi-guest-agent.exe" uninstall'
    File "flexvdi-guest-agent.exe"
    nsExec::ExecToStack '"$INSTDIR\flexvdi-guest-agent.exe" install'
    Pop $0
    ${If} $0 = 0
        DetailPrint "Guest agent installed successfully"
    ${Else}
        Pop $0
        MessageBox MB_OK|MB_ICONEXCLAMATION "Guest agent failed to install: $0"
    ${EndIf}

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
        DetailPrint "Installed credential provider"
    ${EndIf}

    ; Print driver
    SetOutPath "$INSTDIR\printdriver"
    File "@PROJECT_SOURCE_DIR@/print/windriver/flexvdips.*"
    File "@PROJECT_SOURCE_DIR@/print/windriver/gs9.15/gsdll@WIN_BITS@.dll"
    File "print/windriver/filter.exe"
    SetOutPath "$INSTDIR"
    File "print/windriver/redmon.dll" "print/windriver/setup.dll"
    Rename /REBOOTOK "$INSTDIR\redmon.dll" "$SYSDIR\${REDMON_DLL}"
    System::Call 'setup::installRedMon(w "${REDMON_NAME}", w "${REDMON_DLL}", w "${PORT_NAME}", w .r1) i .r0'
    ${If} $0 = 0
        DetailPrint "Print monitor installed"
        WriteRegStr HKLM "${PORT_CONFIG_KEY}"   "Command" '$INSTDIR\printdriver\filter.exe'
        WriteRegStr HKLM "${PORT_CONFIG_KEY}"   "Arguments" ''
        WriteRegDWORD HKLM "${PORT_CONFIG_KEY}" "RunUser" 0
        DetailPrint "Installing flexVDI printer"
        nsExec::Exec 'rundll32 printui.dll,PrintUIEntry /dl /n "${PDF_PRINTER_NAME}" /q'
        nsExec::Exec 'rundll32 printui.dll,PrintUIEntry /if /f "$INSTDIR\printdriver\flexvdips.inf" /m "flexVDI Printer" /r "${PORT_NAME}"'
        Pop $0
        ${If} $0 = 0
            DetailPrint "flexVDI printer successfully installed"
        ${Else}
            MessageBox MB_OK|MB_ICONEXCLAMATION "flexVDI printer NOT installed (error code $0)."
        ${EndIf}
        System::Call 'setup::renamePrinter(w "flexVDI Printer", w "${PDF_PRINTER_NAME}", w .r1) i .r0'
    ${Else}
        MessageBox MB_OK|MB_ICONEXCLAMATION "Print monitor installation failed with code $0: $1"
    ${EndIf}

    ; Compute installed size
    ${GetSize} "$INSTDIR" "/S=0K" $0 $1 $2
    IntFmt $0 "0x%08X" $0
    WriteRegDWORD HKLM "${UNINSTALL_KEY}" "EstimatedSize" "$0"
SectionEnd

Function .onInit
    SectionSetFlags ${install_section_id} ${SF_SELECTED}
    ${IfNot} ${AtLeastWinXP}
        MessageBox MB_OK "XP and above required"
        Quit
    ${EndIf}
FunctionEnd


Section "Uninstall"
    SetAutoClose true
    ${DisableX64FSRedirection}

    nsExec::Exec 'rundll32 printui.dll,PrintUIEntry /dl /n "${PDF_PRINTER_NAME}" /q'
    nsExec::Exec 'rundll32 printui.dll,PrintUIEntry /dd /m "flexVDI Printer" /q'
    SetOutPath "$INSTDIR"
    System::Call 'setup::uninstallRedMon(w "${REDMON_NAME}", w .r1) i .r0 u'
    ${If} $0 = 0
        Delete /REBOOTOK "$SYSDIR\redmon.dll"
    ${Else}
        DetailPrint "ERROR Printer monitor could not be removed: $1"
    ${EndIf}
    SetPluginUnload manual
    System::Free 0

    ${If} ${IsWinXP}
        ReadRegStr $0 HKLM "${UNINSTALL_KEY}" "OldGinaDLL"
        WriteRegStr HKLM "${WINLOGON_KEY}" "GinaDLL" "$0"
        Delete /REBOOTOK $SYSDIR\flexVDIGina.dll
    ${Else}
        DetailPrint "Removed credential provider"
    ${EndIf}

    DeleteRegKey HKLM "${UNINSTALL_KEY}"
    nsExec::Exec '"$INSTDIR\flexvdi-guest-agent.exe" uninstall'
    Pop $0
    ${If} $0 = 0
        DetailPrint "Guest agent service removed"
        RMDir /r /REBOOTOK "$INSTDIR"
    ${Else}
        Pop $0
        MessageBox MB_OK|MB_ICONEXCLAMATION "Guest agent failed to uninstall: $0"
    ${EndIf}
SectionEnd
