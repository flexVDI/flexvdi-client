;
; Copyright Flexible Software Solutions S.L. 2014
;

!include FileFunc.nsh
!include Sections.nsh
!include WinVer.nsh
!include x64.nsh
!include cred-connectors/credential-provider/setup.nsi
!include cred-connectors/gina/setup.nsi
!include print/windriver/setup.nsi

!define APPNAME "flexVDI guest agent"
!define UNINSTALL_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}"

Name "${APPNAME} v@FLEXVDI_VERSION@"
OutFile "flexVDI_setup@WIN_BITS@_@FLEXVDI_VERSION@.exe"
InstallDir $PROGRAMFILES@WIN_BITS@\flexVDI
InstallDirRegKey HKLM "Software\${APPNAME}" "Install_Dir"
RequestExecutionLevel admin
ShowInstDetails show
ShowUninstDetails show
SetOverwrite try
SetPluginUnload alwaysoff

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
        !insertmacro installGina
    ${Else}
        !insertmacro installCredProvider
    ${EndIf}

    ; Print driver
    !insertmacro installPrintDriver

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

    ${If} ${IsWinXP}
        !insertmacro uninstallGina
    ${Else}
        !insertmacro uninstallCredProvider
    ${EndIf}

    !insertmacro uninstallPrintDriver

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
