@echo off
chcp 65001 >nul
setlocal
set "HERE=%~dp0"
echo BodySync: removing the SteamVR driver registration / удаление регистрации драйвера
"%HERE%bodysync_setup.exe" unregister "%HERE%driver\bodysync"
echo.
echo Your settings, profiles and reports are kept in %LOCALAPPDATA%\BodySync
echo Ваши настройки, профили и отчёты хранятся в %LOCALAPPDATA%\BodySync
choice /C YN /D N /T 20 /M "Delete them too? Default is No after 20 s / Удалить и их? По умолчанию Нет через 20 с"
if errorlevel 2 goto keep
rmdir /s /q "%LOCALAPPDATA%\BodySync"
echo Deleted / Удалено.
:keep
echo You can now delete this folder / Теперь можно удалить эту папку.
pause
