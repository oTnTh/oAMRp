EXTPATH=..\ext
CCODEPATH=$(EXTPATH)\c-code
F2KSDKPATH=$(EXTPATH)\f2kSDK
CC=cl.exe
CFLAGS=-nologo -DWIN32 -DNDEBUG -D_WINDOWS -D_WIN32_WINNT=0x501 -EHsc -DUNICODE -LD -O2 -I$(F2KSDKPATH) -I$(EXTPATH)
LINK32=link.exe
RM=del

F2KOBJS = abort_callback.obj album_art.obj app_close_blocker.obj audio_chunk.obj audio_chunk_channel_config.obj cfg_var.obj chapterizer.obj commandline.obj completion_notify.obj config_object.obj console.obj dsp.obj dsp_manager.obj file_info.obj file_info_impl.obj file_info_merge.obj file_operation_callback.obj filesystem.obj filesystem_helper.obj guids.obj hasher_md5.obj input.obj input_file_type.obj link_resolver.obj mainmenu.obj mem_block_container.obj menu_helpers.obj menu_item.obj menu_manager.obj metadb.obj metadb_handle.obj metadb_handle_list.obj packet_decoder.obj playable_location.obj playback_control.obj playlist.obj playlist_loader.obj popup_message.obj preferences_page.obj replaygain.obj replaygain_info.obj service.obj stdafx.obj tag_processor.obj tag_processor_id3v2.obj threaded_process.obj titleformat.obj ui.obj

FCCOBJS = component_client.obj

#profiler.obj
PFCOBJS = bsearch.obj guid.obj other.obj pathUtils.obj printf.obj selftest.obj sort.obj stdafx.obj string.obj string_conv.obj stringNew.obj threads.obj utf8.obj

CCODEOBJS = interf_dec.obj sp_dec.obj

all: foo_input_amr.dll

foo_input_amr.dll: ccode.lib f2k.lib fcc.lib pfc.lib $(CCODEOBJS)
	$(CC) $(CFLAGS) -c $*.cpp
	$(LINK32) -dll -out:$@ $*.obj ccode.lib $(F2KSDKPATH)\foobar2000\shared\shared.lib f2k.lib pfc.lib fcc.lib user32.lib

ccode.lib: $(CCODEOBJS)
	$(LINK32) -lib -out:$@ $(CCODEOBJS)

$(CCODEOBJS):
	$(CC) $(CFLAGS) -c $(CCODEPATH)\$*.c

f2k.lib: $(F2KOBJS)
	$(LINK32) -lib -out:$@ $(F2KOBJS)

$(F2KOBJS):
	$(CC) $(CFLAGS) -c $(F2KSDKPATH)\foobar2000\SDK\$*.cpp

fcc.lib: $(FCCOBJS)
	$(LINK32) -lib -out:$@ $(FCCOBJS)

$(FCCOBJS):
	$(CC) $(CFLAGS) -c $(F2KSDKPATH)\foobar2000\foobar2000_component_client\$*.cpp

pfc.lib: $(PFCOBJS)
	$(LINK32) -lib -out:$@ $(PFCOBJS)

$(PFCOBJS):
	$(CC) $(CFLAGS) -c $(F2KSDKPATH)\pfc\$*.cpp

.PHONY: dev
dev:
	$(CC) $(CFLAGS) -c foo_input_amr.cpp
	$(LINK32) -dll foo_input_amr.obj ccode.lib $(F2KSDKPATH)\foobar2000\shared\shared.lib f2k.lib pfc.lib fcc.lib user32.lib

.PHONY: clean
clean:
	$(RM) *.obj *.exp