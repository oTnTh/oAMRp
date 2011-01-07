EXTPATH=..\ext
CCODEPATH=$(EXTPATH)\c-code
CC=cl.exe
CFLAGS=-nologo -O2 -G6 -MD -I$(EXTPATH) -I$(CCODEPATH)
LINK32=link.exe -nologo
RM=del

CCODEOBJS = interf_dec.obj sp_dec.obj

in_amr.dll: $(CCODEOBJS)
	$(CC) $(CFLAGS) -c $*.c
	$(LINK32) -dll -out:$@ $*.obj $(CCODEOBJS) kernel32.lib user32.lib

$(CCODEOBJS):
	$(CC) $(CFLAGS) -c $(CCODEPATH)\$*.c

all: in_amr.dll

.PHONY: clean
clean:
	$(RM) $(CCODEOBJS) in_amr.obj in_amr.exp in_amr.lib in_amr.dll