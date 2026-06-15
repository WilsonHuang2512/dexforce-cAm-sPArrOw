@echo off
setlocal EnableExtensions

@REM 设置编码为UTF-8, 避免中文乱码
chcp 65001 >nul || goto :fail

cd /d "%~dp0" || goto :fail

@REM 下载firmware的三方库
echo [INFO] 下载 3rdparty_firmware.zip...
curl -f -L -o "3rdparty_firmware.zip" "http://192.168.3.120:80/DexSense/dependencies/cameras/firmware/SPARROW/3rdparty_firmware_v1.2.zip"
if errorlevel 1 goto :fail

@REM 解压三方库到临时文件夹
echo [INFO] unzip 3rdparty_firmware.zip...
if exist "temp_extract_firmware" (
    rmdir /S /Q "temp_extract_firmware"
    if exist "temp_extract_firmware" goto :fail
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "Expand-Archive -Path '3rdparty_firmware.zip' -DestinationPath 'temp_extract_firmware' -Force"
if errorlevel 1 goto :fail

@REM 删除旧的CPU版和GPU版3rdparty文件夹
if exist "firmware_CPU\CPU\3rdparty" (
    echo [INFO] 找到旧的 CPU 3rdparty 文件夹, 删除中...
    rmdir /S /Q "firmware_CPU\CPU\3rdparty"
    if exist "firmware_CPU\CPU\3rdparty" (
        echo [ERROR] 旧的 CPU 3rdparty 文件夹未删除.
        exit /b 1
    ) else (
        echo [INFO] 旧的 CPU 3rdparty 文件夹已删除.
    )
)

if exist "firmware_GPU\GPU\3rdparty" (
    echo [INFO] 找到旧的 GPU 3rdparty 文件夹, 删除中...
    rmdir /S /Q "firmware_GPU\GPU\3rdparty"
    if exist "firmware_GPU\GPU\3rdparty" (
        echo [ERROR] 旧的 GPU 3rdparty 文件夹未删除.
        exit /b 1
    ) else (
        echo [INFO] 旧的 GPU 3rdparty 文件夹已删除.
    )
)

if not exist "firmware_CPU\CPU\3rdparty" mkdir "firmware_CPU\CPU\3rdparty" || goto :fail
if not exist "firmware_GPU\GPU\3rdparty" mkdir "firmware_GPU\GPU\3rdparty" || goto :fail

@REM 拷贝三方库到CPU版和GPU版firmware
echo [INFO] 拷贝三方库到 CPU 目录...
robocopy "temp_extract_firmware\3rdparty" "firmware_CPU\CPU\3rdparty" /E
set "ROBOCOPY_RC=%ERRORLEVEL%"
if %ROBOCOPY_RC% GEQ 8 (
    echo [ERROR] robocopy 到 firmware_CPU\CPU\3rdparty 失败, errorlevel=%ROBOCOPY_RC%
    exit /b %ROBOCOPY_RC%
)

echo [INFO] 拷贝三方库到 GPU 目录...
robocopy "temp_extract_firmware\3rdparty" "firmware_GPU\GPU\3rdparty" /E
set "ROBOCOPY_RC=%ERRORLEVEL%"
if %ROBOCOPY_RC% GEQ 8 (
    echo [ERROR] robocopy 到 firmware_GPU\GPU\3rdparty 失败, errorlevel=%ROBOCOPY_RC%
    exit /b %ROBOCOPY_RC%
)

@REM 删除临时文件和三方库压缩包
if exist "temp_extract_firmware" (
    rmdir /S /Q "temp_extract_firmware"
    if exist "temp_extract_firmware" goto :fail
)

if exist "3rdparty_firmware.zip" (
    del /F /Q "3rdparty_firmware.zip" || goto :fail
)

echo [INFO] 生成 version...
cd /d "%~dp0version" || goto :fail
call build.bat
if errorlevel 1 goto :fail
cd /d "%~dp0" || goto :fail

@REM 进入GPU版camera_server目录
cd /d "%~dp0firmware_GPU\GPU\camera_server" || goto :fail

@REM 删除旧的build文件夹
if exist "build" (
    echo [INFO] 找到旧的 GPU build 文件夹, 删除中...
    rmdir /S /Q "build"
    if exist "build" (
        echo [ERROR] 旧的 GPU build 文件夹未删除.
        exit /b 1
    ) else (
        echo [INFO] 旧的 GPU build 文件夹已删除.
    )
)

if not exist "build" mkdir "build" || goto :fail
cd /d "build" || goto :fail

echo [INFO] 配置 GPU CMake...
cmake ..
if errorlevel 1 (
    echo [ERROR] GPU configure failed
    exit /b %ERRORLEVEL%
)

echo [INFO] 编译 GPU Release...
cmake --build . --config Release
if errorlevel 1 (
    echo [ERROR] GPU build failed
    exit /b %ERRORLEVEL%
)

@REM 进入CPU版camera_server目录
cd /d "%~dp0firmware_CPU\CPU\camera_server_CPU" || goto :fail

@REM 删除旧的build文件夹
if exist "build" (
    echo [INFO] 找到旧的 CPU build 文件夹, 删除中...
    rmdir /S /Q "build"
    if exist "build" (
        echo [ERROR] 旧的 CPU build 文件夹未删除.
        exit /b 1
    ) else (
        echo [INFO] 旧的 CPU build 文件夹已删除.
    )
)

if not exist "build" mkdir "build" || goto :fail
cd /d "build" || goto :fail

echo [INFO] 配置 CPU CMake...
cmake ..
if errorlevel 1 (
    echo [ERROR] CPU configure failed
    exit /b %ERRORLEVEL%
)

echo [INFO] 编译 CPU Release...
cmake --build . --config Release
if errorlevel 1 (
    echo [ERROR] CPU build failed
    exit /b %ERRORLEVEL%
)

cd /d "%~dp0" || goto :fail

if exist "SPARROW_Firmware" (
    rmdir /S /Q "SPARROW_Firmware"
    if exist "SPARROW_Firmware" goto :fail
)

if exist "SPARROW_Firmware.zip" (
    del /F /Q "SPARROW_Firmware.zip" || goto :fail
)

if not exist "SPARROW_Firmware" mkdir "SPARROW_Firmware" || goto :fail
if not exist "SPARROW_Firmware\GPU" mkdir "SPARROW_Firmware\GPU" || goto :fail
if not exist "SPARROW_Firmware\CPU" mkdir "SPARROW_Firmware\CPU" || goto :fail

copy /Y "firmware_GPU\GPU\camera_server\build\Release\camera_server_GPU.exe" "SPARROW_Firmware\GPU\" >nul || goto :fail
copy /Y "firmware_CPU\CPU\camera_server_CPU\build\Release\camera_server_CPU.exe" "SPARROW_Firmware\CPU\" >nul || goto :fail

@REM 拷贝GPU版的动态库
echo [INFO] 拷贝 GPU runtime...
robocopy "%~dp0firmware_GPU\GPU\3rdparty\runtime" "%~dp0SPARROW_Firmware\GPU" /E
set "ROBOCOPY_RC=%ERRORLEVEL%"
if %ROBOCOPY_RC% GEQ 8 (
    echo [ERROR] robocopy GPU runtime 失败, errorlevel=%ROBOCOPY_RC%
    exit /b %ROBOCOPY_RC%
)

@REM 拷贝CPU版的动态库
copy /Y "firmware_CPU\CPU\3rdparty\runtime\opencv_world480.dll" "%~dp0SPARROW_Firmware\CPU\" >nul || goto :fail
copy /Y "firmware_CPU\CPU\3rdparty\runtime\GxIAPI.dll" "%~dp0SPARROW_Firmware\CPU\" >nul || goto :fail
copy /Y "firmware_CPU\CPU\3rdparty\runtime\concrt140.dll" "%~dp0SPARROW_Firmware\CPU\" >nul || goto :fail
copy /Y "firmware_CPU\CPU\3rdparty\runtime\msvcp*.dll" "%~dp0SPARROW_Firmware\CPU\" >nul || goto :fail
copy /Y "firmware_CPU\CPU\3rdparty\runtime\vc*.dll" "%~dp0SPARROW_Firmware\CPU\" >nul || goto :fail

echo [INFO] 压缩 SPARROW_Firmware.zip...
powershell -NoProfile -ExecutionPolicy Bypass -Command "Compress-Archive -Path 'SPARROW_Firmware\*' -DestinationPath 'SPARROW_Firmware.zip' -Force"
if errorlevel 1 goto :fail

echo [INFO] finish
exit /b 0

:fail
set "ERR=%ERRORLEVEL%"
if "%ERR%"=="" set "ERR=1"
if "%ERR%"=="0" set "ERR=1"
echo [ERROR] 脚本执行失败，errorlevel=%ERR%
exit /b %ERR%
