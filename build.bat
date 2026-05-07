@echo off
setlocal

set "IMAGE_NAME=xyon-ps2"
if not "%XYON_DOCKER_IMAGE%"=="" set "IMAGE_NAME=%XYON_DOCKER_IMAGE%"

set "EXAMPLE_ID=%~1"
if "%EXAMPLE_ID%"=="" set "EXAMPLE_ID=render_images"

docker build -t "%IMAGE_NAME%" .
if errorlevel 1 exit /b %errorlevel%

if "%EXAMPLE_ID%"=="all" (
    docker run --rm -v "%cd%:/xyon" -w /xyon "%IMAGE_NAME%" make all
) else (
    docker run --rm -v "%cd%:/xyon" -w /xyon "%IMAGE_NAME%" make -C "examples/%EXAMPLE_ID%" all
)
exit /b %errorlevel%
