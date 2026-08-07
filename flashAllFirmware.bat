@echo off
cd /d %~dp0

REM Path without quotes
set JLINK_EXE=C:\Program Files\SEGGER\JLink\JLink.exe
set FLASH_SCRIPT=%~dp0.vscode\flash.jlink

REM Proper quoting at execution
"%JLINK_EXE%" -device S32K144 -if SWD -speed 4000 -autoconnect 1 -CommanderScript "%FLASH_SCRIPT%"


