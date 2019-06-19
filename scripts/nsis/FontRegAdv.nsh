### Modified Code from FileFunc.nsh    ###
### Original by Instructor and kichik  ###
### Modified by Javier Celaya to avoid depending on FontName plugin ###

!ifmacrondef GetFileNameCall
        !macro GetFileNameCall _PATHSTRING _RESULT
                Push `${_PATHSTRING}`
              	Call GetFileName
               	Pop ${_RESULT}
        !macroend
!endif

!ifndef GetFileName
	!define GetFileName `!insertmacro GetFileNameCall`

	Function GetFileName
		Exch $0
		Push $1
		Push $2

		StrCpy $2 $0 1 -1
		StrCmp $2 '\' 0 +3
		StrCpy $0 $0 -1
		goto -3

		StrCpy $1 0
		IntOp $1 $1 - 1
		StrCpy $2 $0 1 $1
		StrCmp $2 '' end
		StrCmp $2 '\' 0 -3
		IntOp $1 $1 + 1
		StrCpy $0 $0 '' $1

		end:
		Pop $2
		Pop $1
		Exch $0
	FunctionEnd
!endif

### End Code From ###

!macro InstallTTF FontFile FontName
  Push $0  
  Push $R0
  Push $R1
  Push $R2
  Push $R3
  
  !define Index 'Line${__LINE__}'

  DetailPrint "Installing font ${FontName}"
  
; Get the Font's File name
  ${GetFileName} ${FontFile} $0
  !define FontFileName $0

  SetOutPath $FONTS
  IfFileExists "$FONTS\${FontFileName}" ${Index} 0
  File '${FontFile}'

${Index}:
  ClearErrors
  ReadRegStr $R0 HKLM "SOFTWARE\Microsoft\Windows NT\CurrentVersion" "CurrentVersion"
  IfErrors "${Index}-9x" "${Index}-NT"

"${Index}-NT:"
  StrCpy $R1 "Software\Microsoft\Windows NT\CurrentVersion\Fonts"
  goto "${Index}-Add"

"${Index}-9x:"
  StrCpy $R1 "Software\Microsoft\Windows\CurrentVersion\Fonts"
    
"${Index}-Add:"
  StrCpy $R2 "${FontName} (TrueType)"
  ClearErrors
  ReadRegStr $R0 HKLM "$R1" "$R2"
  IfErrors 0 "${Index}-End"
    System::Call "GDI32::AddFontResourceA(t) i ('${FontFileName}') .s"
    WriteRegStr HKLM "$R1" "$R2" "${FontFileName}"
    goto "${Index}-End"

"${Index}-End:"

  !undef Index
  !undef FontFileName
  
  pop $R3
  pop $R2
  pop $R1
  Pop $R0  
  Pop $0
!macroend
