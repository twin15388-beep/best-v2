@echo off
setlocal

echo MalinkaAvoidPreview - OBSERVE-ONLY FIRST
echo.
echo Find DDNet.exe PID in Task Manager ^> Details. The command below does NOT enable input.
tasklist /fi "imagename eq DDNet.exe"
echo.
set /p "TARGET_PID=Enter the PID of the official BestClient DDNet.exe: "
if not defined TARGET_PID (
  echo No PID entered.
  pause
  exit /b 2
)
"%~dp0MalinkaAvoidPreview.exe" --pid "%TARGET_PID%" --run-ms 30000
set "PREVIEW_EXIT=%ERRORLEVEL%"
echo.
echo Preview exit code: %PREVIEW_EXIT%
echo Copy any BLOCKED line and the final RESULT line for diagnosis.
pause
exit /b %PREVIEW_EXIT%
