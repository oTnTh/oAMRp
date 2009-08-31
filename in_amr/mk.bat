set EXTPATH=..\ext
cl -I%EXTPATH% -O2 -G6 -MD -c in_amr.c %EXTPATH%\c-code\interf_dec.c %EXTPATH%\c-code\sp_dec.c
link -dll in_amr.obj interf_dec.obj sp_dec.obj kernel32.lib user32.lib
del *.exp *.lib *.obj
upx -9 in_amr.dll