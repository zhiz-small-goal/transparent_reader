@echo off
setlocal

set "ROOT=%~dp0"
if "%ROOT:~-1%"=="\" (
  set "ROOT=%ROOT:~0,-1%"
)
set "BUILD_DIR=%ROOT%\build"
set "CONFIG=Debug"
set "ERROR_CODE=0"

echo Reconfiguring CMake in "%ROOT%"...
cmake -S "%ROOT%" -B "%BUILD_DIR%"
if errorlevel 1 (
  set "ERROR_CODE=1"
  goto :Finalize
)

echo Cleaning previous build artifacts...
cmake --build "%BUILD_DIR%" --config %CONFIG% --target clean
if errorlevel 1 (
  echo Clean step failed (the target may not support clean). Continuing anyway.
)

echo Building %CONFIG% configuration...
cmake --build "%BUILD_DIR%" --config %CONFIG%
if errorlevel 1 (
  set "ERROR_CODE=1"
  goto :Finalize
)

set "EXE=%BUILD_DIR%\bin\%CONFIG%\TransparentMdReader.exe"
if not exist "%EXE%" (
  echo Executable not found at "%EXE%".
  set "ERROR_CODE=1"
  goto :Finalize
)

echo Launching TransparentMdReader...
start "" /D "%BUILD_DIR%\bin\%CONFIG%" "%EXE%"

:Finalize
if "%ERROR_CODE%"=="1" (
  echo.
  echo Errors occurred during the script run.
)
echo.
echo Press any key to exit . . .
pause > nul

set "EXIT_CODE=%ERROR_CODE%"
endlocal
exit /b %EXIT_CODE%
