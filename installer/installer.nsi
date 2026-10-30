; BodySync installer. Build: makensis -DSTAGE=<release folder> -DVERSION=1.0.0 -DOUT=<file.exe> installer.nsi
Unicode true
!include "MUI2.nsh"
!include "LogicLib.nsh"
!include "FileFunc.nsh"

!ifndef VERSION
  !define VERSION "0.0.0"
!endif
!ifndef OUT
  !define OUT "BodySync-Setup.exe"
!endif
!define UNINST_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\BodySync"

Name "BodySync ${VERSION}"
OutFile "${OUT}"
InstallDir "$LOCALAPPDATA\Programs\BodySync"
InstallDirRegKey HKCU "Software\BodySync" "InstallDir"
RequestExecutionLevel user          ; no administrator rights needed: everything lives in the user profile
SetCompressor /SOLID lzma
BrandingText "BodySync ${VERSION}"
VIProductVersion "${VERSION}.0"
VIAddVersionKey "ProductName" "BodySync"
VIAddVersionKey "FileDescription" "BodySync installer"
VIAddVersionKey "FileVersion" "${VERSION}"
VIAddVersionKey "ProductVersion" "${VERSION}"
VIAddVersionKey "LegalCopyright" "Copyright (c) BodySync contributors"

!define MUI_ICON "${STAGE}/BodySync.ico"
!define MUI_UNICON "${STAGE}/BodySync.ico"
!define MUI_ABORTWARNING
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "${STAGE}/LICENSE.txt"
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!define MUI_FINISHPAGE_RUN "$INSTDIR\BodySync.exe"
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"
!insertmacro MUI_LANGUAGE "Russian"
!insertmacro MUI_LANGUAGE "German"
!insertmacro MUI_LANGUAGE "French"
!insertmacro MUI_LANGUAGE "Spanish"

LangString SecMain ${LANG_ENGLISH} "BodySync application (required)"
LangString SecMain ${LANG_RUSSIAN} "Приложение BodySync (обязательно)"
LangString SecMain ${LANG_GERMAN} "BodySync-Anwendung (erforderlich)"
LangString SecMain ${LANG_FRENCH} "Application BodySync (obligatoire)"
LangString SecMain ${LANG_SPANISH} "Aplicación BodySync (obligatoria)"
LangString SecDrv ${LANG_ENGLISH} "Register the BodySync driver in SteamVR"
LangString SecDrv ${LANG_RUSSIAN} "Зарегистрировать драйвер BodySync в SteamVR"
LangString SecDrv ${LANG_GERMAN} "BodySync-Treiber in SteamVR registrieren"
LangString SecDrv ${LANG_FRENCH} "Enregistrer le pilote BodySync dans SteamVR"
LangString SecDrv ${LANG_SPANISH} "Registrar el controlador de BodySync en SteamVR"
LangString SecDesk ${LANG_ENGLISH} "Desktop shortcut"
LangString SecDesk ${LANG_RUSSIAN} "Ярлык на рабочем столе"
LangString SecDesk ${LANG_GERMAN} "Desktopverknüpfung"
LangString SecDesk ${LANG_FRENCH} "Raccourci sur le bureau"
LangString SecDesk ${LANG_SPANISH} "Acceso directo en el escritorio"
LangString RunningMsg ${LANG_ENGLISH} "BodySync is running. Close it and click Retry, or Cancel to stop."
LangString RunningMsg ${LANG_RUSSIAN} "BodySync запущен. Закройте его и нажмите «Повтор», или «Отмена», чтобы прервать."
LangString RunningMsg ${LANG_GERMAN} "BodySync läuft. Beenden Sie es und klicken Sie auf Wiederholen, oder auf Abbrechen, um abzubrechen."
LangString RunningMsg ${LANG_FRENCH} "BodySync est en cours d'exécution. Fermez-le puis cliquez sur Réessayer, ou sur Annuler pour arrêter."
LangString RunningMsg ${LANG_SPANISH} "BodySync se está ejecutando. Ciérralo y pulsa Reintentar, o Cancelar para detener."
LangString NoSteamVR ${LANG_ENGLISH} "The driver could not be registered (SteamVR was not found or has never been started).$\r$\nStart SteamVR once, then run 'Register SteamVR driver' from the Start menu."
LangString NoSteamVR ${LANG_RUSSIAN} "Драйвер не удалось зарегистрировать (SteamVR не найден или ни разу не запускался).$\r$\nОдин раз запустите SteamVR, затем выберите в меню «Пуск» пункт «Register SteamVR driver»."
LangString NoSteamVR ${LANG_GERMAN} "Der Treiber konnte nicht registriert werden (SteamVR wurde nicht gefunden oder noch nie gestartet).$\r$\nStarten Sie SteamVR einmal und wählen Sie dann im Startmenü 'Register SteamVR driver'."
LangString NoSteamVR ${LANG_FRENCH} "Le pilote n'a pas pu être enregistré (SteamVR est introuvable ou n'a jamais été lancé).$\r$\nLancez SteamVR une fois, puis choisissez 'Register SteamVR driver' dans le menu Démarrer."
LangString NoSteamVR ${LANG_SPANISH} "No se pudo registrar el controlador (SteamVR no se encontró o nunca se ha iniciado).$\r$\nInicia SteamVR una vez y luego elige 'Register SteamVR driver' en el menú Inicio."
LangString DrvFail ${LANG_ENGLISH} "The driver could not be registered. See the BodySync documentation (TROUBLESHOOTING)."
LangString DrvFail ${LANG_RUSSIAN} "Не удалось зарегистрировать драйвер. См. документацию BodySync (TROUBLESHOOTING)."
LangString DrvFail ${LANG_GERMAN} "Der Treiber konnte nicht registriert werden. Siehe BodySync-Dokumentation (TROUBLESHOOTING)."
LangString DrvFail ${LANG_FRENCH} "Le pilote n'a pas pu être enregistré. Voir la documentation BodySync (TROUBLESHOOTING)."
LangString DrvFail ${LANG_SPANISH} "No se pudo registrar el controlador. Consulta la documentación de BodySync (TROUBLESHOOTING)."
LangString KeepData ${LANG_ENGLISH} "Also delete your BodySync settings, profiles and reports ($LOCALAPPDATA\BodySync)?$\r$\nChoose No to keep them."
LangString KeepData ${LANG_RUSSIAN} "Удалить также настройки, профили и отчёты BodySync ($LOCALAPPDATA\BodySync)?$\r$\nВыберите «Нет», чтобы сохранить их."
LangString KeepData ${LANG_GERMAN} "Auch Ihre BodySync-Einstellungen, Profile und Berichte löschen ($LOCALAPPDATA\BodySync)?$\r$\nWählen Sie Nein, um sie zu behalten."
LangString KeepData ${LANG_FRENCH} "Supprimer aussi vos réglages, profils et rapports BodySync ($LOCALAPPDATA\BodySync) ?$\r$\nChoisissez Non pour les conserver."
LangString KeepData ${LANG_SPANISH} "¿Eliminar también tus ajustes, perfiles e informes de BodySync ($LOCALAPPDATA\BodySync)?$\r$\nElige No para conservarlos."

Function .onInit
  retry:
  FindWindow $0 "BodySyncWnd"
  ${If} $0 != 0
    IfSilent abortit
    MessageBox MB_RETRYCANCEL|MB_ICONEXCLAMATION "$(RunningMsg)" IDRETRY retry
    abortit:
    Abort
  ${EndIf}
FunctionEnd

Section "$(SecMain)" SEC_MAIN
  SectionIn RO
  SetOutPath "$INSTDIR"
  File /r "${STAGE}/*.*"
  WriteUninstaller "$INSTDIR\Uninstall-BodySync.exe"
  WriteRegStr HKCU "Software\BodySync" "InstallDir" "$INSTDIR"
  WriteRegStr HKCU "${UNINST_KEY}" "DisplayName" "BodySync"
  WriteRegStr HKCU "${UNINST_KEY}" "DisplayVersion" "${VERSION}"
  WriteRegStr HKCU "${UNINST_KEY}" "Publisher" "BodySync"
  WriteRegStr HKCU "${UNINST_KEY}" "InstallLocation" "$INSTDIR"
  WriteRegStr HKCU "${UNINST_KEY}" "DisplayIcon" "$INSTDIR\BodySync.exe"
  WriteRegStr HKCU "${UNINST_KEY}" "UninstallString" '"$INSTDIR\Uninstall-BodySync.exe"'
  WriteRegStr HKCU "${UNINST_KEY}" "QuietUninstallString" '"$INSTDIR\Uninstall-BodySync.exe" /S'
  WriteRegDWORD HKCU "${UNINST_KEY}" "NoModify" 1
  WriteRegDWORD HKCU "${UNINST_KEY}" "NoRepair" 1
  CreateDirectory "$SMPROGRAMS\BodySync"
  CreateShortcut "$SMPROGRAMS\BodySync\BodySync.lnk" "$INSTDIR\BodySync.exe"
  CreateShortcut "$SMPROGRAMS\BodySync\Register SteamVR driver.lnk" "$INSTDIR\Install.bat"
  CreateShortcut "$SMPROGRAMS\BodySync\Uninstall BodySync.lnk" "$INSTDIR\Uninstall-BodySync.exe"
SectionEnd

Section "$(SecDrv)" SEC_DRV
  ExecWait '"$INSTDIR\bodysync_setup.exe" register "$INSTDIR\driver\bodysync"' $0
  ${If} $0 == 2
    IfSilent done
    MessageBox MB_OK|MB_ICONINFORMATION "$(NoSteamVR)"
  ${ElseIf} $0 != 0
    IfSilent done
    MessageBox MB_OK|MB_ICONEXCLAMATION "$(DrvFail)"
  ${EndIf}
  done:
SectionEnd

Section /o "$(SecDesk)" SEC_DESK
  CreateShortcut "$DESKTOP\BodySync.lnk" "$INSTDIR\BodySync.exe"
SectionEnd

Section "Uninstall"
  FindWindow $0 "BodySyncWnd"
  ${If} $0 != 0
    MessageBox MB_OK|MB_ICONEXCLAMATION "$(RunningMsg)"
    Abort
  ${EndIf}
  ExecWait '"$INSTDIR\bodysync_setup.exe" unregister "$INSTDIR\driver\bodysync"' $0
  Delete "$SMPROGRAMS\BodySync\*.lnk"
  RMDir "$SMPROGRAMS\BodySync"
  Delete "$DESKTOP\BodySync.lnk"
  ; only what we installed, never "RMDir /r $INSTDIR" (the user may have chosen a folder with other files)
  Delete "$INSTDIR\BodySync.exe"
  Delete "$INSTDIR\bodysync_setup.exe"
  Delete "$INSTDIR\openvr_api.dll"
  Delete "$INSTDIR\BodySync.ico"
  Delete "$INSTDIR\Install.bat"
  Delete "$INSTDIR\Uninstall.bat"
  Delete "$INSTDIR\*.md"
  Delete "$INSTDIR\LICENSE.txt"
  RMDir /r "$INSTDIR\LICENSES"
  Delete "$INSTDIR\VERSION.txt"
  RMDir /r "$INSTDIR\driver"
  RMDir /r "$INSTDIR\fonts"
  RMDir /r "$INSTDIR\docs"
  Delete "$INSTDIR\Uninstall-BodySync.exe"
  RMDir "$INSTDIR"
  DeleteRegKey HKCU "${UNINST_KEY}"
  DeleteRegKey HKCU "Software\BodySync"
  ; user data is removed only with explicit consent: interactive answer, or /PURGE on the command line
  ${GetParameters} $1
  ClearErrors
  ${GetOptions} $1 "/PURGE" $2
  ${IfNot} ${Errors}
    Goto purge
  ${EndIf}
  IfSilent keep
  MessageBox MB_YESNO|MB_ICONQUESTION|MB_DEFBUTTON2 "$(KeepData)" IDNO keep
  purge:
  RMDir /r "$LOCALAPPDATA\BodySync"
  keep:
SectionEnd
