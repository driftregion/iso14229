@echo off
setlocal enabledelayedexpansion

REM Get commit hash
for /f "tokens=*" %%i in ('git rev-parse --short HEAD 2^>nul') do set commit_hash=%%i
if "%commit_hash%"=="" set commit_hash=unknown
echo STABLE_SCM_REVISION %commit_hash%

REM Check if working directory is dirty
git diff --quiet >nul 2>&1
if errorlevel 1 (
    echo STABLE_SCM_DIRTY .dirty
) else (
    echo STABLE_SCM_DIRTY
)

REM Extract version from README.md
for /f "tokens=2" %%i in ('findstr /r "^## [0-9]*\.[0-9]*\.[0-9]*" README.md 2^>nul') do (
    set version=%%i
    goto :version_found
)
set version=
:version_found
echo STABLE_README_VERSION %version%