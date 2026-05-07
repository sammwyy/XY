@echo off
setlocal

call "%~dp0build.bat" all
exit /b %errorlevel%

