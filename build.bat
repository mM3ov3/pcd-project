@echo off
REM Set working directory to the script location (root project directory)
cd /d %~dp0

REM Create and activate a virtual environment in the root project directory
IF NOT EXIST "venv" (
    echo Creating virtual environment...
    python -m venv .\build\venv
)

echo Activating virtual environment...
call .\build\venv\Scripts\activate.bat

REM Install dependencies (e.g., pyinstaller) from the requirements.txt file in the root directory
echo Installing dependencies...
pip install -r requirements.txt

REM Check if PyInstaller is installed
echo Checking for PyInstaller...
pip show pyinstaller > nul
IF %ERRORLEVEL% NEQ 0 (
    echo PyInstaller is not installed. Installing it now...
    pip install pyinstaller
)

REM Package the Python code using PyInstaller
echo Packaging the Python client with PyInstaller...
pyinstaller --onefile --distpath build --workpath build/py-client --specpath build/py-client src/py-client/py-client.py

REM Clean up (optional)
echo Cleaning up temporary files...
rd /s /q build/py-client

REM Deactivate virtual environment
echo Deactivating virtual environment...
deactivate

echo Build process completed. The executable is in the build directory.
pause
