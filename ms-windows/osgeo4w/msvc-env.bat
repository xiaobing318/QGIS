@echo off
REM ***************************************************************************
REM    msvc-env.cmd
REM    ---------------------
REM    begin                : June 2018
REM    copyright            : (C) 2018 by Juergen E. Fischer
REM    email                : jef at norbit dot de
REM ***************************************************************************
REM *                                                                         *
REM *   This program is free software; you can redistribute it and/or modify  *
REM *   it under the terms of the GNU General Public License as published by  *
REM *   the Free Software Foundation; either version 2 of the License, or     *
REM *   (at your option) any later version.                                   *
REM *                                                                         *
REM ***************************************************************************

if not "%PROGRAMFILES(X86)%"=="" set PF86=%PROGRAMFILES(X86)%
if "%PF86%"=="" set PF86=%PROGRAMFILES%
if "%PF86%"=="" (echo PROGRAMFILES not set & goto error)
REM 杨小兵-2024-11-04（输出%PF86%）


REM 杨小兵-2024-01-11（获得系统信息）
REM if "%PF%"=="" set PF=%PROGRAMFILES%

REM 杨小兵-2024-01-11（设置MSVC中SDK的版本信息）
if "%VCSDK%"=="" set VCSDK=10.0.19041.0


set ARCH=%1
if "%ARCH%"=="x86" goto x86
if "%ARCH%"=="x86_64" goto x86_64
goto usage

:x86
set VCARCH=x86
set CMAKE_COMPILER_PATH=%PF86%\Microsoft Visual Studio 14.0\VC\bin
set DBGHLP_PATH=%PF86%\Microsoft Visual Studio 14.0\Common7\IDE\Remote Debugger\x86
set SETUPAPI_LIBRARY=%PF86%\Windows Kits\10\Lib\%VCSDK%\um\x86\SetupAPI.Lib
goto archset

:x86_64
set VCARCH=amd64
set CMAKE_COMPILER_PATH=%PF86%\Microsoft Visual Studio\2019\Community\VC\Tools\MSVC\14.29.30133\bin\Hostx64\x64
set DBGHLP_PATH=%PF86%\Microsoft Visual Studio\2019\Community\Common7\IDE\Remote Debugger\x64
set SETUPAPI_LIBRARY=%PF86%\Windows Kits\10\Lib\%VCSDK%\um\x64\SetupAPI.Lib

:archset
if not exist "%SETUPAPI_LIBRARY%" (echo SETUPAPI_LIBRARY not found & goto error)

if "%CC%"=="" set CC=%CMAKE_COMPILER_PATH:\=/%/cl.exe
if "%CXX%"=="" set CXX=%CMAKE_COMPILER_PATH:\=/%/cl.exe
set CLCACHE_CL=%CMAKE_COMPILER_PATH:\=/%/cl.exe

if "%OSGEO4W_ROOT%"=="" if "%ARCH%"=="x86" (
	set OSGEO4W_ROOT=C:\OSGeo4W
) else (
	set OSGEO4W_ROOT=C:\OSGeo4W64
)

if not exist "%OSGEO4W_ROOT%\bin\o4w_env.bat" (echo o4w_env.bat not found & goto error)
call "%OSGEO4W_ROOT%\bin\o4w_env.bat"
call "%OSGEO4W_ROOT%\bin\py3_env.bat"
call "%OSGEO4W_ROOT%\bin\qt5_env.bat"

set VS160COMNTOOLS=%PF86%\Microsoft Visual Studio\2019\Community\Common7\Tools\
call "%PF86%\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat" %VCARCH%

path %path%;%PF86%\Microsoft Visual Studio\2019\Community\VC\Tools\MSVC\14.29.30133\bin\Hostx64\x64

set GRASS8=
if exist %OSGEO4W_ROOT%\bin\grass83.bat set GRASS8=%OSGEO4W_ROOT%\bin\grass83.bat
if "%GRASS8%"=="" (echo GRASS8 not found & goto error)
for /f "usebackq tokens=1" %%a in (`%GRASS8% --config path`) do set GRASS_PREFIX=%%a

REM set GRASS7=
REM if exist %OSGEO4W_ROOT%\bin\grass74.bat set GRASS7=%OSGEO4W_ROOT%\bin\grass74.bat
REM if exist %OSGEO4W_ROOT%\bin\grass76.bat set GRASS7=%OSGEO4W_ROOT%\bin\grass76.bat
REM if exist %OSGEO4W_ROOT%\bin\grass78.bat set GRASS7=%OSGEO4W_ROOT%\bin\grass78.bat
REM if "%GRASS7%"=="" (echo GRASS7 not found & goto error)
REM for /f "usebackq tokens=1" %%a in (`%GRASS7% --config path`) do set GRASS_PREFIX=%%a
REM 杨小兵-2024-01-12（判断GRASS8是否被设置)
if "%GRASS8%"=="" (echo GRASS8 not found & goto error)
for /f "usebackq tokens=1" %%a in (`%GRASS8% --config path`) do set GRASS_PREFIX=%%a

set PYTHONPATH=
if exist "%PROGRAMFILES%\CMake\bin" path %PATH%;%PROGRAMFILES%\CMake\bin
if exist "%PF86%\CMake\bin" path %PATH%;%PF86%\CMake\bin
if exist c:\cygwin64\bin path %PATH%;c:\cygwin64\bin
if exist c:\cygwin\bin path %PATH%;c:\cygwin\bin

set LIB=%LIB%;%OSGEO4W_ROOT%\apps\Qt5\lib;%OSGEO4W_ROOT%\lib
set INCLUDE=%INCLUDE%;%OSGEO4W_ROOT%\apps\Qt5\include;%OSGEO4W_ROOT%\include

goto end

:usage
echo usage: %0 [x86^|x86_64]
echo sample: %0 x86_64
exit /b 1

:error
echo ENV ERROR %ERRORLEVEL%: %DATE% %TIME%
exit /b 1

:end