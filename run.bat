@echo off
start "" "python" "python/face_tracker.py"
timeout /t 2 /nobreak >nul
.\build\WallpaperENGINE.exe
