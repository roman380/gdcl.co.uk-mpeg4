; Installation Script (C) 2015 RenderHeads Ltd.  All Rights Reserved.
; Update for repository fork at https://github.com/roman380/gdcl.co.uk-mpeg4
; ________________________________________________________________________

!define     PRODUCTNAME         "GDCL MPEG-4 Filters"
!define     PRODUCTTITLE        "GDCL DirectShow MPEG-4 Part 14 (.MP4) Multiplexer and Demultiplexer Filters"
!define     SHORTVERSION        "1.0.0"
!define     TITLE				"${PRODUCTTITLE} ${SHORTVERSION}"

SetCompressor /Solid lzma
RequestExecutionLevel admin

InstallDir "$PROGRAMFILES\${PRODUCTNAME}"

!include x64.nsh

Function .onInit
	UserInfo::GetAccountType
	pop $0
	${If} $0 != "admin" ;Require admin rights on NT4+
	    MessageBox mb_iconstop "Administrator rights required!"
	    SetErrorLevel 740 ;ERROR_ELEVATION_REQUIRED
	    Quit
	${EndIf}
FunctionEnd

Name "${TITLE}"
Caption "${TITLE}"

OutFile "..\_Bin\GDCL-MPEG4CodecSetup.exe"

XPStyle on

; _____________________________
; Install Pages
;

PageEx instfiles
    Caption " - Installing"
PageExEnd

; ____________________________
; Program Files Operations
;

Section "FilesInstall"
	SetShellVarContext all

	; 64-bit install
	${if} ${RunningX64}
		SetOutPath $INSTDIR\x64
		File "..\_Bin\x64\Release\mp4mux.dll"
		File "..\_Bin\x64\Release\mp4demux.dll"
		ExecWait 'regsvr32.exe /s "$OUTDIR\mp4mux.dll"'
		ExecWait 'regsvr32.exe /s "$OUTDIR\mp4demux.dll"'
	${else}
	${endif}

	SetOutPath $INSTDIR\Win32
	File "..\_Bin\Win32\Release\mp4mux.dll"
	File "..\_Bin\Win32\Release\mp4demux.dll"
	ExecWait 'regsvr32.exe /s "$OUTDIR\mp4mux.dll"'
	ExecWait 'regsvr32.exe /s "$OUTDIR\mp4demux.dll"'

SectionEnd


; _____________________________
; Registry Operations
;

Section "Registry"
	;MessageBox MB_OK "registry"
	SetOutPath $INSTDIR

	; Write the uninstall keys for Windows
	; NOTE: https://msdn.microsoft.com/en-us/library/windows/desktop/aa372105
	WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCTNAME}" "DisplayName" "${PRODUCTNAME}"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCTNAME}" "DisplayVersion" "1.0.0"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCTNAME}" "Publisher" 'GDCL'
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCTNAME}" "Version" 0x01000000
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCTNAME}" "VersionMajor" 1
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCTNAME}" "VersionMinor" 0
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCTNAME}" "EstimatedSize" 1024
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCTNAME}" "NoModify" 1
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCTNAME}" "NoRepair" 1
	WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCTNAME}" "URLInfoAbout" "https://github.com/roman380/gdcl.co.uk-mpeg4"
	WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCTNAME}" "UninstallString" '"$INSTDIR\uninstall.exe"'
   	WriteUninstaller "$INSTDIR\uninstall.exe"

	CreateDirectory "$SMPROGRAMS\${PRODUCTNAME}"
	CreateShortCut "$SMPROGRAMS\${PRODUCTNAME}\Uninstall.lnk" "$INSTDIR\uninstall.exe"
SectionEnd

; _______________________
; Uninstall
;

UninstPage uninstConfirm
UninstPage instfiles

Section "Uninstall"

	SetShellVarContext all

	${if} ${RunningX64}	
		ExecWait 'regsvr32.exe /u /s "$INSTDIR\x64\mp4mux.dll"'
		ExecWait 'regsvr32.exe /u /s "$INSTDIR\x64\mp4demux.dll"'
	${else}
	${endif}
	ExecWait 'regsvr32.exe /u /s "$INSTDIR\Win32\mp4mux.dll"'
	ExecWait 'regsvr32.exe /u /s "$INSTDIR\Win32\mp4demux.dll"'

	; remove registry keys
	DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCTNAME}"

    ; remove the links from the start menu
    RMDir /r "$SMPROGRAMS\${PRODUCTNAME}"

    RMDir /r "$INSTDIR"	
SectionEnd