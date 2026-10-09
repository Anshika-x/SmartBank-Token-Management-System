@echo off
title SmartBank C Server
echo ==========================================
echo        SMARTBANK C WEBSITE SERVER
echo ==========================================
echo.
gcc server.c -o server.exe -lws2_32
if errorlevel 1 (
  echo.
  echo Build failed. Check that GCC is installed in the MSYS2 UCRT64 environment.
  pause
  exit /b 1
)
echo.
echo Build successful. Starting SmartBank...
echo Open http://localhost:8080 in your browser.
echo Keep this window open while using the website.
echo Press Ctrl+C to stop the server.
echo.
server.exe
pause
