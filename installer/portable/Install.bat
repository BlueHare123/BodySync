@echo off
chcp 65001 >nul
setlocal
set "HERE=%~dp0"
echo BodySync: registering the SteamVR driver / регистрация драйвера SteamVR
echo Folder / Папка: %HERE%driver\bodysync
echo Keep this folder where it is: SteamVR loads the driver from here.
echo Не перемещайте эту папку: SteamVR загружает драйвер отсюда.
echo.
"%HERE%bodysync_setup.exe" register "%HERE%driver\bodysync"
if errorlevel 1 (
  echo.
  echo Registration failed. See docs\TROUBLESHOOTING.md / Регистрация не удалась. См. docs\TROUBLESHOOTING.md
  pause
  exit /b 1
)
echo.
echo Done. Restart SteamVR if it is running, then start BodySync.exe
echo Готово. Перезапустите SteamVR, если он запущен, затем запустите BodySync.exe
pause
