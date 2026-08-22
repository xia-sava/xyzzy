@echo off
setlocal
cd /d %~dp0

set VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe
if not exist "%VSWHERE%" (
  echo vswhere.exe not found. Visual Studio 2017 or later is required.
  exit /b 1
)
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set VSINSTALL=%%i
if not defined VSINSTALL (
  echo Visual C++ build tools not found.
  exit /b 1
)
call "%VSINSTALL%\VC\Auxiliary\Build\vcvars32.bat" || exit /b 1

if "%1"=="" (set CONFIGURATION=Release) else (set CONFIGURATION=%1)
if "%2"=="" (set TARGET=Build) else (set TARGET=%2)
if "%3"=="" (set VERBOSITY=normal) else (set VERBOSITY=%3)

echo Build xyzzy...
msbuild xyzzy.sln /nologo /p:Configuration=%CONFIGURATION% /p:Platform=Win32 /target:%TARGET% /verbosity:%VERBOSITY% %4 %5 %6 %7 %8 %9 || exit /b 1
