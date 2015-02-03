;
; Copyright Flexible Software Solutions S.L. 2014
;

!include cred-connectors/credential-provider/setup.nsi
!include cred-connectors/gina/setup.nsi
!include print/windriver/setup.nsi
!define UNINSTALL_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}"

!macro installMacro
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

    ; Agent
    nsExec::Exec '"$INSTDIR\flexvdi-guest-agent.exe" uninstall'
    ${If} ${RunningX64}
        File "../win64/flexvdi-guest-agent.exe"
    ${Else}
        File "../win32/flexvdi-guest-agent.exe"
    ${EndIf}
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
!macroend

!macro uninstallMacro
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
!macroend
