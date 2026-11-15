@echo off
setlocal enabledelayedexpansion

REM Release helper script for Synth Audio Synthesizer
REM Usage: create_release.bat <version>
REM Example: create_release.bat v1.0.0

if "%~1"=="" (
    echo Usage: %0 ^<version^>
    echo Example: %0 v1.0.0
    exit /b 1
)

set VERSION=%~1

REM Basic version validation
echo %VERSION% | findstr /r "^v[0-9][0-9]*\.[0-9][0-9]*\.[0-9][0-9]*" >nul
if errorlevel 1 (
    echo Error: Version must be in format v1.2.3 or v1.2.3-alpha
    echo Examples: v1.0.0, v1.2.3, v2.0.0-beta
    exit /b 1
)

echo Creating release %VERSION% for Synth Audio Synthesizer

REM Check if we're in a git repository
git rev-parse --git-dir >nul 2>&1
if errorlevel 1 (
    echo Error: Not in a git repository
    exit /b 1
)

REM Check if working directory is clean
git diff-index --quiet HEAD -- >nul 2>&1
if errorlevel 1 (
    echo Error: Working directory is not clean. Please commit your changes first.
    git status --porcelain
    exit /b 1
)

REM Check current branch
for /f "tokens=*" %%i in ('git rev-parse --abbrev-ref HEAD') do set CURRENT_BRANCH=%%i
if not "%CURRENT_BRANCH%"=="main" (
    echo Warning: You're not on the main branch ^(current: %CURRENT_BRANCH%^)
    set /p REPLY="Continue anyway? (y/N): "
    if /i not "!REPLY!"=="y" exit /b 1
)

REM Check if tag already exists
git rev-parse %VERSION% >nul 2>&1
if not errorlevel 1 (
    echo Error: Tag %VERSION% already exists
    exit /b 1
)

REM Create and push tag
echo Creating and pushing tag %VERSION%...
git tag -a "%VERSION%" -m "Release %VERSION%"
git push origin "%VERSION%"

echo.
echo ✅ Release %VERSION% created successfully!
echo.
echo The GitHub Action will now:
echo 1. Build the project for Windows and Linux
echo 2. Run tests to ensure quality
echo 3. Package build artifacts with documentation
echo 4. Create a GitHub release with download links
echo.
echo You can monitor the progress at:
for /f "tokens=*" %%i in ('git config remote.origin.url') do (
    set REPO_URL=%%i
    set REPO_URL=!REPO_URL:https://github.com/=!
    set REPO_URL=!REPO_URL:.git=!
    echo https://github.com/!REPO_URL!/actions
    echo.
    echo The release will be available at:
    echo https://github.com/!REPO_URL!/releases/tag/%VERSION%
)

endlocal
