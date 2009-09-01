wget -c http://www.3gpp.org/ftp/Specs/archive/26_series/26.104/26104-800.zip
7z x -oOut 26104-800.zip
7z x -oOut Out\26104-800_ANSI_C_source_code.zip
move Out\c-code .
rmdir /s/q Out
del 26104-800.zip

wget -c http://download.nullsoft.com/winamp/plugin-dev/WA5.55_SDK.exe
7z x -oOut WA5.55_SDK.exe
move Out\Winamp WinampSDK
rmdir /s/q Out
del WA5.55_SDK.exe

wget -c http://www.foobar2000.org/files/916db17a3687fbf14f95b8edd245e5ee/SDK-2008-11-29.zip
7z x -of2kSDK SDK-2008-11-29.zip
del SDK-2008-11-29.zip