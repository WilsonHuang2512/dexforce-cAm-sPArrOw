@echo off
setlocal EnableExtensions

@REM 设置编码为UTF-8, 避免中文乱码
chcp 65001 >nul || goto :fail

cd /d "%~dp0" || goto :fail

echo [INFO] 下载 3rdparty.zip...
curl -f -L -o "3rdparty.zip" "http://192.168.3.120:80/DexSense/dependencies/cameras/win/SPARROW/3rdparty_v1.1.zip"
if errorlevel 1 goto :fail

if not exist "temp_extract" (
    mkdir "temp_extract" || goto :fail
)

echo [INFO] 解压到临时目录...
powershell -NoProfile -ExecutionPolicy Bypass -Command "Expand-Archive -Path '3rdparty.zip' -DestinationPath 'temp_extract' -Force"
if errorlevel 1 goto :fail

@REM 创建 SPARROW\3rdparty 目录
if exist "3rdparty" (
    echo [INFO] 找到旧的 3rdparty 文件夹, 删除中...
    rmdir /S /Q "3rdparty"
    if exist "3rdparty" (
        echo [ERROR] 旧的 3rdparty 文件夹未删除.
        exit /b 1
    )
    echo [INFO] 旧的 3rdparty 文件夹已删除.
)

if not exist "3rdparty" (
    mkdir "3rdparty" || goto :fail
)

echo [INFO] 移动解压内容到 3rdparty...
robocopy "temp_extract\3rdparty" "3rdparty" /E /MOVE
set "ROBOCOPY_RC=%ERRORLEVEL%"
if %ROBOCOPY_RC% GEQ 8 (
    echo [ERROR] robocopy temp_extract\3rdparty -> 3rdparty 失败, errorlevel=%ROBOCOPY_RC%
    exit /b %ROBOCOPY_RC%
)

@REM 删除临时文件和三方库压缩包
if exist "temp_extract" (
    rmdir /S /Q "temp_extract"
    if exist "temp_extract" goto :fail
)

if exist "3rdparty.zip" (
    del /F /Q "3rdparty.zip" || goto :fail
)

echo [INFO] 生成 version...
cd /d "%~dp0version" || goto :fail
call build.bat
if errorlevel 1 goto :fail
cd /d "%~dp0" || goto :fail

@REM 删除旧的build文件夹
if exist "build" (
    echo [INFO] 找到旧的 build 文件夹, 删除中...
    rmdir /S /Q "build"
    if exist "build" (
        echo [ERROR] 旧的 build 文件夹未删除.
        exit /b 1
    )
    echo [INFO] 旧的 build 文件夹已删除.
)

@REM 创建 build 目录
if not exist "build" (
    mkdir "build" || goto :fail
)

@REM 进入 build 目录
cd /d "build" || goto :fail

@REM 运行 CMake 配置
echo [INFO] CMake configure...
cmake ..
if errorlevel 1 (
    echo [ERROR] config failed
    exit /b %ERRORLEVEL%
)

@REM 编译 Release
echo [INFO] CMake build...
cmake --build . --config Release
if errorlevel 1 (
    echo [ERROR] build failed
    exit /b %ERRORLEVEL%
)

echo [INFO] CMake install...
cmake --install . --config Release
if errorlevel 1 (
    echo [ERROR] install failed
    exit /b %ERRORLEVEL%
)

@REM 进入 cpp 文件夹
cd /d "%~dp0cpp" || goto :fail

echo [INFO] 执行 neutral.py...
python neutral.py
if errorlevel 1 (
    echo [ERROR] neutral failed
    exit /b %ERRORLEVEL%
)

cd /d "%~dp0" || goto :fail

if exist "SPARROW_Release" (
    rmdir /S /Q "SPARROW_Release"
    if exist "SPARROW_Release" goto :fail
)

if exist "SPARROW_Release.zip" (
    del /F /Q "SPARROW_Release.zip" || goto :fail
)

if not exist "SPARROW_Release" (
    mkdir "SPARROW_Release" || goto :fail
)

cd /d "%~dp0SPARROW_Release" || goto :fail

@REM 创建三种SDK包
if not exist "SPARROW_neutral" mkdir "SPARROW_neutral" || goto :fail
if not exist "SPARROW_core" mkdir "SPARROW_core" || goto :fail
if not exist "SPARROW_pickwiz" mkdir "SPARROW_pickwiz" || goto :fail

@REM 开始SPARROW_neutral的文件夹创建打包
cd /d "%~dp0SPARROW_Release\SPARROW_neutral" || goto :fail
if not exist "lib" mkdir "lib" || goto :fail
if not exist "GUI" mkdir "GUI" || goto :fail
if not exist "doc" mkdir "doc" || goto :fail
if not exist "example" mkdir "example" || goto :fail
cd /d "lib" || goto :fail
if not exist "cpp" mkdir "cpp" || goto :fail
if not exist "python" mkdir "python" || goto :fail
if not exist "c#" mkdir "c#" || goto :fail
cd /d "..\example" || goto :fail
if not exist "cpp_example" mkdir "cpp_example" || goto :fail
if not exist "python_example" mkdir "python_example" || goto :fail
if not exist "c#_example" mkdir "c#_example" || goto :fail

@REM 开始SPARROW_core的文件夹创建打包
cd /d "%~dp0SPARROW_Release\SPARROW_core" || goto :fail
if not exist "lib" mkdir "lib" || goto :fail
if not exist "GUI" mkdir "GUI" || goto :fail
if not exist "doc" mkdir "doc" || goto :fail
if not exist "example" mkdir "example" || goto :fail
cd /d "lib" || goto :fail
if not exist "cpp" mkdir "cpp" || goto :fail
if not exist "python" mkdir "python" || goto :fail
if not exist "c#" mkdir "c#" || goto :fail
cd /d "..\example" || goto :fail
if not exist "cpp_example" mkdir "cpp_example" || goto :fail
if not exist "python_example" mkdir "python_example" || goto :fail
if not exist "c#_example" mkdir "c#_example" || goto :fail

@REM 开始SPARROW_pickwiz的文件夹创建打包
cd /d "%~dp0SPARROW_Release\SPARROW_pickwiz" || goto :fail
if not exist "lib" mkdir "lib" || goto :fail
if not exist "example" mkdir "example" || goto :fail

@REM 开始文件copy
copy /Y "%~dp0build\install\lib\sparrow_core.lib" "%~dp0SPARROW_Release\SPARROW_core\lib\cpp\" >nul || goto :fail
copy /Y "%~dp0build\install\bin\sparrow_core.dll" "%~dp0SPARROW_Release\SPARROW_core\lib\cpp\" >nul || goto :fail
copy /Y "%~dp0build\install\lib\sparrow_core.lib" "%~dp0SPARROW_Release\SPARROW_core\example\cpp_example\" >nul || goto :fail
copy /Y "%~dp0build\install\bin\sparrow_core.dll" "%~dp0SPARROW_Release\SPARROW_core\example\cpp_example\" >nul || goto :fail
copy /Y "%~dp0cpp\scamera.h" "%~dp0SPARROW_Release\SPARROW_core\example\cpp_example\" >nul || goto :fail
copy /Y "%~dp0cpp_example\cpp_example.cpp" "%~dp0SPARROW_Release\SPARROW_core\example\cpp_example\" >nul || goto :fail
copy /Y "%~dp0cpp_example\cpp_example.vcxproj" "%~dp0SPARROW_Release\SPARROW_core\example\cpp_example\" >nul || goto :fail
copy /Y "%~dp0cpp_example\cpp_example.vcxproj.filters" "%~dp0SPARROW_Release\SPARROW_core\example\cpp_example\" >nul || goto :fail
copy /Y "%~dp0cpp_example\cpp_example.vcxproj.user" "%~dp0SPARROW_Release\SPARROW_core\example\cpp_example\" >nul || goto :fail
copy /Y "%~dp0cmd\warm-up.txt" "%~dp0SPARROW_Release\SPARROW_core\GUI\" >nul || goto :fail

@REM 其他工具到GUI文件夹
robocopy "%~dp0build\install\bin" "%~dp0SPARROW_Release\SPARROW_core\GUI" /E
set "ROBOCOPY_RC=%ERRORLEVEL%"
if %ROBOCOPY_RC% GEQ 8 (
    echo [ERROR] robocopy build\install\bin -> SPARROW_core\GUI 失败, errorlevel=%ROBOCOPY_RC%
    exit /b %ROBOCOPY_RC%
)

copy /Y "%~dp0build\install\lib\camera.lib" "%~dp0SPARROW_Release\SPARROW_neutral\lib\cpp\" >nul || goto :fail
copy /Y "%~dp0build\install\bin\camera.dll" "%~dp0SPARROW_Release\SPARROW_neutral\lib\cpp\" >nul || goto :fail
copy /Y "%~dp0build\install\lib\camera.lib" "%~dp0SPARROW_Release\SPARROW_neutral\example\cpp_example\" >nul || goto :fail
copy /Y "%~dp0build\install\bin\camera.dll" "%~dp0SPARROW_Release\SPARROW_neutral\example\cpp_example\" >nul || goto :fail
copy /Y "%~dp0cpp\scamera_neutral.h" "%~dp0SPARROW_Release\SPARROW_neutral\example\cpp_example\" >nul || goto :fail
copy /Y "%~dp0cpp_example\cpp_example.cpp" "%~dp0SPARROW_Release\SPARROW_neutral\example\cpp_example\" >nul || goto :fail
copy /Y "%~dp0cpp_example\cpp_example.vcxproj" "%~dp0SPARROW_Release\SPARROW_neutral\example\cpp_example\" >nul || goto :fail
copy /Y "%~dp0cpp_example\cpp_example.vcxproj.filters" "%~dp0SPARROW_Release\SPARROW_neutral\example\cpp_example\" >nul || goto :fail
copy /Y "%~dp0cpp_example\cpp_example.vcxproj.user" "%~dp0SPARROW_Release\SPARROW_neutral\example\cpp_example\" >nul || goto :fail

copy /Y "%~dp0build\install\lib\sparrow_cpp.lib" "%~dp0SPARROW_Release\SPARROW_pickwiz\lib\" >nul || goto :fail
copy /Y "%~dp0build\install\bin\sparrow_cpp.dll" "%~dp0SPARROW_Release\SPARROW_pickwiz\lib\" >nul || goto :fail
copy /Y "%~dp0build\install\lib\sparrow_cpp.lib" "%~dp0SPARROW_Release\SPARROW_pickwiz\example\" >nul || goto :fail
copy /Y "%~dp0build\install\bin\sparrow_cpp.dll" "%~dp0SPARROW_Release\SPARROW_pickwiz\example\" >nul || goto :fail
copy /Y "%~dp0cpp\scamera.h" "%~dp0SPARROW_Release\SPARROW_pickwiz\example\" >nul || goto :fail
copy /Y "%~dp0cpp_example\cpp_example.cpp" "%~dp0SPARROW_Release\SPARROW_pickwiz\example\" >nul || goto :fail
copy /Y "%~dp0cpp_example\cpp_example.vcxproj" "%~dp0SPARROW_Release\SPARROW_pickwiz\example\" >nul || goto :fail
copy /Y "%~dp0cpp_example\cpp_example.vcxproj.filters" "%~dp0SPARROW_Release\SPARROW_pickwiz\example\" >nul || goto :fail
copy /Y "%~dp0cpp_example\cpp_example.vcxproj.user" "%~dp0SPARROW_Release\SPARROW_pickwiz\example\" >nul || goto :fail

@REM 修改部分文件
cd /d "%~dp0" || goto :fail
set "CPP_FILE=SPARROW_Release\SPARROW_neutral\example\cpp_example\cpp_example.cpp"
set "VCXPROJ_FILE=SPARROW_Release\SPARROW_neutral\example\cpp_example\cpp_example.vcxproj"
powershell -NoProfile -ExecutionPolicy Bypass -Command "(Get-Content '%CPP_FILE%') -replace '#include \"scamera.h\"', '#include \"scamera_neutral.h\"' -replace 'SparrowEngine', 'Engine' -replace 'SparrowColor', 'Color' -replace 'SPARROW', 'CAMERA' | Set-Content '%CPP_FILE%'"
if errorlevel 1 goto :fail
powershell -NoProfile -ExecutionPolicy Bypass -Command "(Get-Content '%VCXPROJ_FILE%') -replace 'sparrow_cpp.lib', 'camera.lib' | Set-Content '%VCXPROJ_FILE%'"
if errorlevel 1 goto :fail

set "VCXPROJ_FILE=SPARROW_Release\SPARROW_core\example\cpp_example\cpp_example.vcxproj"
powershell -NoProfile -ExecutionPolicy Bypass -Command "(Get-Content '%VCXPROJ_FILE%') -replace 'sparrow_cpp.lib', 'sparrow_core.lib' | Set-Content '%VCXPROJ_FILE%'"
if errorlevel 1 goto :fail

set "VCXPROJ_FILE=SPARROW_Release\SPARROW_pickwiz\example\cpp_example.vcxproj"
powershell -NoProfile -ExecutionPolicy Bypass -Command "(Get-Content '%VCXPROJ_FILE%') -replace 'NDEBUG;', 'NDEBUG;USE_OPENCV;' | Set-Content '%VCXPROJ_FILE%'"
if errorlevel 1 goto :fail

echo [INFO] wait 10 seconds for file copy to complete...
powershell -NoProfile -ExecutionPolicy Bypass -Command "Start-Sleep -Seconds 10"
if errorlevel 1 goto :fail

@REM 打包
echo [INFO] 压缩 SPARROW_Release.zip...
powershell -NoProfile -ExecutionPolicy Bypass -Command "Compress-Archive -Path 'SPARROW_Release\*' -DestinationPath 'SPARROW_Release.zip' -Force"
if errorlevel 1 goto :fail

echo [INFO] finish
exit /b 0

:fail
set "ERR=%ERRORLEVEL%"
if "%ERR%"=="" set "ERR=1"
if "%ERR%"=="0" set "ERR=1"
echo [ERROR] 脚本执行失败，errorlevel=%ERR%
exit /b %ERR%
