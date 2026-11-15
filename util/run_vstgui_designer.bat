@echo off
echo Starting VSTGUI Designer...
echo.
echo This tool provides a visual interface for designing VST plugin UIs.
echo.
echo Navigation:
echo - Left Panel: Control palette and canvas settings
echo - Center: Visual design canvas with drag and drop
echo - Right Panel: Properties for selected elements
echo.
echo Workflow:
echo 1. Set canvas size (e.g., 500x400 for VST)
echo 2. Click controls in left panel to add elements
echo 3. Drag elements to position them
echo 4. Select elements and edit properties on the right
echo 5. Export XML when finished
echo.
echo The generated .uidesc file should be saved to:
echo src\vst\resource\editor.uidesc
echo.
pause
python vstgui_designer.py
pause