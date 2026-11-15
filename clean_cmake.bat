@echo off
echo Cleaning CMake build files...

if exist build (
    echo Removing build directory...
    rmdir /s /q build
    echo Build directory removed.
) else (
    echo Build directory not found.
)

echo Clean completed.
