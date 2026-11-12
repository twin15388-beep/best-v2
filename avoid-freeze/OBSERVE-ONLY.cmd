@echo off
setlocal

echo AvoidFreeze - observe-only direction advice; no game input is sent.
echo This is an experimental build. Do not use it on a public server.
echo.
tasklist /fi "imagename eq DDNet.exe"
echo.
set /p "TARGET_PID=Enter the PID of the unchanged official DDNet.exe: "
if not defined TARGET_PID (
  echo No PID entered.
  pause
  exit /b 2
)
"%~dp0AvoidFreeze.exe" --observe-only --pid "%TARGET_PID%" --run-ms 30000
set "PREVIEW_EXIT=%ERRORLEVEL%"
echo.
echo Preview exit code: %PREVIEW_EXIT%
echo Copy the final RESULT line and any BLOCKED line. Remove any private details before sharing.
pause
exit /b %PREVIEW_EXIT%
