@echo off
setlocal
pushd %~dp0

rem install mingw-gcc 4.8+

set MINGW_HOME=C:\mingw
set MINGW_BIN=%MINGW_HOME%\bin
set MINGW_LIB32=%MINGW_HOME%\i686-w64-mingw32\lib
set MINGW_LIB64=%MINGW_HOME%\x86_64-w64-mingw32\lib

set CORE_FILES=^
src\fs-poll.c ^
src\inet.c ^
src\threadpool.c ^
src\uv-common.c ^
src\version.c ^
src\win\async.c ^
src\win\core.c ^
src\win\detect-wakeup.c ^
src\win\dl.c ^
src\win\error.c ^
src\win\fs.c ^
src\win\fs-event.c ^
src\win\getaddrinfo.c ^
src\win\getnameinfo.c ^
src\win\handle.c ^
src\win\loop-watcher.c ^
src\win\pipe.c ^
src\win\thread.c ^
src\win\poll.c ^
src\win\process.c ^
src\win\process-stdio.c ^
src\win\req.c ^
src\win\signal.c ^
src\win\snprintf.c ^
src\win\stream.c ^
src\win\tcp.c ^
src\win\tty.c ^
src\win\timer.c ^
src\win\udp.c ^
src\win\util.c ^
src\win\winapi.c ^
src\win\winsock.c ^
src\jni.c

set LIB32_FILES=^
%MINGW_LIB32%\libws2_32.a ^
%MINGW_LIB32%\libpsapi.a ^
%MINGW_LIB32%\libiphlpapi.a ^
%MINGW_LIB32%\libuserenv.a

set LIB64_FILES=^
%MINGW_LIB64%\libws2_32.a ^
%MINGW_LIB64%\libpsapi.a ^
%MINGW_LIB64%\libiphlpapi.a ^
%MINGW_LIB64%\libuserenv.a

set COMPILE=-std=gnu89 -DNDEBUG -DWIN32 -D_WIN32_WINNT=0x0600 -D__USE_MINGW_ANSI_STDIO=1 -D_GNU_SOURCE -DBUILDING_UV_SHARED -DENABLE_JNI -Iinclude -Isrc -Isrc/jni -Ofast -ffast-math -fweb -fomit-frame-pointer -fmerge-all-constants -flto -fwhole-program -pipe -static

set COMPILE32=%MINGW_BIN%\i686-w64-mingw32-gcc.exe -m32 -march=i686 %COMPILE% -Wl,--enable-stdcall-fixup
set COMPILE64=%MINGW_BIN%\x86_64-w64-mingw32-gcc.exe -m64 %COMPILE%

echo building uvjni32.dll ...
%COMPILE32% -shared -Wl,--image-base,0x10000000 -Wl,--kill-at -Wl,--compat-implib -Wl,--out-implib,uvjni32.lib -Wl,-soname -Wl,uvjni32.dll -o uvjni32.dll %CORE_FILES% %LIB32_FILES% uvjni.def

echo building uvjni64.dll ...
%COMPILE64% -shared -Wl,--image-base,0x10000000 -Wl,--kill-at -Wl,--compat-implib -Wl,--out-implib,uvjni64.lib -Wl,-soname -Wl,uvjni64.dll -o uvjni64.dll %CORE_FILES% %LIB64_FILES% uvjni.def

pause
