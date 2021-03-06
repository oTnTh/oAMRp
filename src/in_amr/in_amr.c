// oAMRp - AMR Plugin for Winamp
// Using GPL v2
// Author: http://otnth.blogspot.com

#include <windows.h>
#include <string.h>

#include <WinampSDK/IN2.h>

#include "c-code/interf_dec.h"
#include "c-code/sp_dec.h"
#include "c-code/typedef.h"


// avoid CRT. Evil. Big. Bloated. Only uncomment this code if you are using 
// 'ignore default libraries' in VC++. Keeps DLL size way down.
// /*
BOOL WINAPI _DllMainCRTStartup(HANDLE hInst, ULONG ul_reason_for_call, LPVOID lpReserved) {
	return TRUE;
}
// */

#define AMR_MAGIC_NUMBER "#!AMR\n"

// post this to the main window at end of file (after playback as stopped)
#define WM_WA_MPEG_EOF WM_USER+2

// raw configuration.
#define NCH 1
#define SAMPLERATE 8000
#define BPS 16

In_Module mod;			// the output module (filled in near the bottom of this file)

char lastfn[MAX_PATH];	// currently playing file (used for getting info on the current file)
int file_length;		// file length, in bytes

int frame_count;		// sum of frames
int frame_cur;			// current frame

int paused;				// are we paused?
volatile int seek_needed; // if != -1, it is the point that the decode 
						  // thread should seek to, in ms.

HANDLE input_file=INVALID_HANDLE_VALUE; // input file handle

volatile int killDecodeThread=0;			// the kill switch for the decode thread
HANDLE thread_handle=INVALID_HANDLE_VALUE;	// the handle to the decode thread

DWORD WINAPI DecodeThread(LPVOID b); // the decode thread procedure

// 800
int * decoder_state;
const short block_size[16]={ 12, 13, 15, 17, 19, 20, 26, 31, 5, 0, 0, 0, 0, 0, 0, 0 };
///////////////////////////////////////////////
//int decode_pos_ms;		// current decoding position, in milliseconds. 
						// Used for correcting DSP plug-in pitch changes

void config(HWND hwndParent)
{
	MessageBox(hwndParent,
		"No configuration. ONLY support AMR IF1 files now.",
		"Configuration",MB_OK);
	// if we had a configuration box we'd want to write it here (using DialogBox, etc)
}
void about(HWND hwndParent)
{
	MessageBox(hwndParent,"oTnTh AMR Plugin, http://otnth.blogspot.com",
		"About oTnTh AMR Plugin",MB_OK);
}

void init() { 
	/* any one-time initialization goes here (configuration reading, etc) */ 
}

void quit() { 
	/* one-time deinit, such as memory freeing */ 
}

int isourfile(const char *fn) {
	// 判断是否为AMR文件，是则返回1
	// 如果以后添加了IF2等其他格式，可直接返回0
	int l = 0;
	char* magic[8];
	HANDLE fh;
	
	if (!strnicmp(&fn[strlen(fn)-4], ".amr", 4)) {
		fh = CreateFile(fn,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,
			OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
		if (!fh) return 0;

		// 读文件标志
		ReadFile(fh, magic, strlen(AMR_MAGIC_NUMBER), &l, NULL);
		CloseHandle(fh);

		if (l != strlen(AMR_MAGIC_NUMBER) ||
			strncmp(magic, AMR_MAGIC_NUMBER, strlen(AMR_MAGIC_NUMBER)) ) return 0;

		return 1;
	} else {
		return 0;
	}
}

// 文件定位，dest为目标frame序数
// 为-1时返回文件内的frame总数
int seek_file(HANDLE fh, int dest) {
	int l, p, n;
	char c;

	// 保存读写位置
	p = SetFilePointer(fh, 0, NULL, FILE_CURRENT);

	if (dest == -1) {
		// 从文件头开始定位
		SetFilePointer(fh, strlen(AMR_MAGIC_NUMBER), NULL, FILE_BEGIN);
	}
	
	n = 0;
	// 计算frame总数
	while (ReadFile(fh, &c, 1, &l, NULL) && l == 1) {
		l = block_size[(c >> 3) & 0x000F];
		l = SetFilePointer(fh, l, NULL, FILE_CURRENT);
		n++;
		
		if (n == dest) break;
	}

	// 还原读写位置
	if (dest == -1) SetFilePointer(fh, p, NULL, FILE_BEGIN);

	return n;
}

// called when winamp wants to play a file
int play(const char *fn) 
{ 
	int maxlatency;
	int thread_id;
	
	// 800
	int l;
	char magic[8];

	paused=0;
	seek_needed=-1;
	
	// CHANGEME! Write your own file opening code here
	input_file = CreateFile(fn,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,
		OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
	if (input_file == INVALID_HANDLE_VALUE) // error opening file
	{
		// we return error. 1 means to keep going in the playlist, -1
		// means to stop the playlist.
		return 1;
	}
	
	// Check IF1 file format
	ReadFile(input_file, magic, strlen(AMR_MAGIC_NUMBER), &l, NULL);
	if ( l != strlen(AMR_MAGIC_NUMBER) ||
		strncmp(magic, AMR_MAGIC_NUMBER, strlen(AMR_MAGIC_NUMBER)) ) {
		CloseHandle(input_file);
		return 1;
	}

	file_length=GetFileSize(input_file,NULL);

	strcpy(lastfn,fn);

	frame_cur = 0;
	frame_count = seek_file(input_file, -1);

	// init decoer
	decoder_state = Decoder_Interface_init();

	// -1 and -1 are to specify buffer and prebuffer lengths.
	// -1 means to use the default, which all input plug-ins should
	// really do.
	maxlatency = mod.outMod->Open(SAMPLERATE,NCH,BPS, -1,-1); 

	// maxlatency is the maxium latency between a outMod->Write() call and
	// when you hear those samples. In ms. Used primarily by the visualization
	// system.

	if (maxlatency < 0) // error opening device
	{
		CloseHandle(input_file);
		input_file=INVALID_HANDLE_VALUE;
		return 1;
	}
	// dividing by 1000 for the first parameter of setinfo makes it
	// display 'H'... for hundred.. i.e. 14H Kbps.
	mod.SetInfo((SAMPLERATE*BPS*NCH)/1000,SAMPLERATE/1000,NCH,1);

	// initialize visualization stuff
	mod.SAVSAInit(maxlatency,SAMPLERATE);
	mod.VSASetInfo(SAMPLERATE,NCH);

	// set the output plug-ins default volume.
	// volume is 0-255, -666 is a token for
	// current volume.
	mod.outMod->SetVolume(-666); 

	// launch decode thread
	killDecodeThread=0;
	thread_handle = (HANDLE) 
		CreateThread(NULL,0,(LPTHREAD_START_ROUTINE) DecodeThread,NULL,0,&thread_id);
	
	return 0; 
}

// standard pause implementation
void pause() { paused=1; mod.outMod->Pause(1); }
void unpause() { paused=0; mod.outMod->Pause(0); }
int ispaused() { return paused; }


// stop playing.
void stop() { 
	Decoder_Interface_exit(decoder_state);
	
	if (thread_handle != INVALID_HANDLE_VALUE)
	{
		killDecodeThread=1;
		if (WaitForSingleObject(thread_handle,10000) == WAIT_TIMEOUT)
		{
			MessageBox(mod.hMainWindow,"error asking thread to die!\n",
				"error killing decode thread",0);
			TerminateThread(thread_handle,0);
		}
		CloseHandle(thread_handle);
		thread_handle = INVALID_HANDLE_VALUE;
	}

	// close output system
	mod.outMod->Close();

	// deinitialize visualization
	mod.SAVSADeInit();
	

	// CHANGEME! Write your own file closing code here
	if (input_file != INVALID_HANDLE_VALUE)
	{
		CloseHandle(input_file);
		input_file=INVALID_HANDLE_VALUE;
	}

}


// returns length of playing track
int getlength() {
	// 每一frame包含20ms的数据
	return frame_count*20;
}


// returns current output position, in ms.
// you could just use return mod.outMod->GetOutputTime(),
// but the dsp plug-ins that do tempo changing tend to make
// that wrong.
int getoutputtime() { 
	/*
	return decode_pos_ms+
		(mod.outMod->GetOutputTime()-mod.outMod->GetWrittenTime()); 
	*/
	// ?!
	return frame_cur*20+
		(mod.outMod->GetOutputTime()-mod.outMod->GetWrittenTime()); 
}


// called when the user releases the seek scroll bar.
// usually we use it to set seek_needed to the seek
// point (seek_needed is -1 when no seek is needed)
// and the decode thread checks seek_needed.
void setoutputtime(int time_in_ms) { 
	seek_needed=time_in_ms; 
}


// standard volume/pan functions
void setvolume(int volume) { mod.outMod->SetVolume(volume); }
void setpan(int pan) { mod.outMod->SetPan(pan); }

// this gets called when the use hits Alt+3 to get the file info.
// if you need more info, ask me :)

int infoDlg(const char *fn, HWND hwnd)
{
	// CHANGEME! Write your own info dialog code here
	return 0;
}


// this is an odd function. it is used to get the title and/or
// length of a track.
// if filename is either NULL or of length 0, it means you should
// return the info of lastfn. Otherwise, return the information
// for the file in filename.
// if title is NULL, no title is copied into it.
// if length_in_ms is NULL, no length is copied into it.
void getfileinfo(const char *filename, char *title, int *length_in_ms)
{
	if (!filename || !*filename)  // currently playing file
	{
		if (length_in_ms) *length_in_ms=getlength();
		if (title) // get non-path portion.of filename
		{
			char *p=lastfn+strlen(lastfn);
			while (*p != '\\' && p >= lastfn) p--;
			strcpy(title,++p);
		}
	}
	else // some other file
	{
		if (length_in_ms) // calculate length
		{
			HANDLE hFile;
			hFile = CreateFile(filename,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,
				OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
			if (hFile != INVALID_HANDLE_VALUE)
			{
				SetFilePointer(hFile, strlen(AMR_MAGIC_NUMBER), NULL, FILE_BEGIN);
				*length_in_ms = seek_file(hFile, -1)*20;
				CloseHandle(hFile);
			}
			else *length_in_ms=-1000; // the default is unknown file length (-1000).
		}
		if (title) // get non path portion of filename
		{
			const char *p=filename+strlen(filename);
			while (*p != '\\' && p >= filename) p--;
			strcpy(title,++p);
		}
	}
}

void eq_set(int on, char data[10], int preamp) 
{ 
	// most plug-ins can't even do an EQ anyhow.. I'm working on writing
	// a generic PCM EQ, but it looks like it'll be a little too CPU 
	// consuming to be useful :)
	// if you _CAN_ do EQ with your format, each data byte is 0-63 (+20db <-> -20db)
	// and preamp is the same. 
}


// render 576 samples into buf. 
// this function is only used by DecodeThread. 

// note that if you adjust the size of sample_buffer, for say, 1024
// sample blocks, it will still work, but some of the visualization 
// might not look as good as it could. Stick with 576 sample blocks
// if you can, and have an additional auxiliary (overflow) buffer if 
// necessary.. 
/*
int get_576_samples(char *buf)
{
	int l;
	// CHANGEME! Write your own sample getting code here
	ReadFile(input_file,buf,576*NCH*(BPS/8),&l,NULL);
	return l;
}
*/


DWORD WINAPI DecodeThread(LPVOID b)
{
	int done=0; // set to TRUE if decoding has finished
	
	// 800
	short synth[160];
	unsigned char analysis[32];
	int read_size;
	enum Mode dec_mode;
	
	while (!killDecodeThread) 
	{
		if (seek_needed != -1) // seek is needed.
		{
			mod.outMod->Flush(seek_needed);   // flush output device and set 
											  // output position to the seek position
			frame_cur = seek_needed / 20;
			seek_needed = -1;
			done = 0;
			
			seek_file(input_file, frame_cur); // 定位
		}

		if (done) // done was set to TRUE during decoding, signaling eof
		{
			mod.outMod->CanWrite();		// some output drivers need CanWrite
									    // to be called on a regular basis.

			if (!mod.outMod->IsPlaying()) 
			{
				// we're done playing, so tell Winamp and quit the thread.
				PostMessage(mod.hMainWindow,WM_WA_MPEG_EOF,0,0);
				return 0;	// quit thread
			}
			Sleep(10);		// give a little CPU time back to the system.
		}
		// 800
		else if (mod.outMod->CanWrite() >= 320*(mod.dsp_isactive()?2:1))
		//else if (mod.outMod->CanWrite() >= ((576*NCH*(BPS/8))*(mod.dsp_isactive()?2:1)))
			// CanWrite() returns the number of bytes you can write, so we check that
			// to the block size. the reason we multiply the block size by two if 
			// mod.dsp_isactive() is that DSP plug-ins can change it by up to a 
			// factor of two (for tempo adjustment).
		{
			ReadFile(input_file, analysis, 1, &read_size, NULL);
			if (read_size <= 0) { // no samples to play
				done = 1;
			} else {
				// read file
				dec_mode = (analysis[0] >> 3) & 0x000F;
				read_size = block_size[dec_mode];
				ReadFile(input_file, &analysis[1], read_size, &read_size, NULL);
				
				// call decoder
				Decoder_Interface_Decode(decoder_state, analysis, synth, 0);
				
				// give the samples to the vis subsystems
				// 示波器，每次至少添加576个sample的数据，但1个frame只有160个
				//mod.SAAddPCMData((char *)synth,NCH,BPS,frame_cur*20);	
				//mod.VSAAddPCMData((char *)synth,NCH,BPS,frame_cur*20);
				// adjust decode position variable
				frame_cur++;

				read_size = 160*2;
				// if we have a DSP plug-in, then call it on our samples
				if (mod.dsp_isactive()) 
					read_size=mod.dsp_dosamples(
						(short *)synth,320/NCH/(BPS/8),BPS,NCH,SAMPLERATE
					  ) // dsp_dosamples
					  *(NCH*(BPS/8));

				// write the pcm data to the output system
				mod.outMod->Write((char*)synth, read_size);
			}
		}
		else Sleep(20); 
		// if we can't write data, wait a little bit. Otherwise, continue 
		// through the loop writing more data (without sleeping)
	}
	return 0;
}


// module definition.

In_Module mod = 
{
	IN_VER,	// defined in IN2.H
	"oTnTh AMR Plugin v0.1 "
	// winamp runs on both alpha systems and x86 ones. :)
#ifdef __alpha
	"(AXP)"
#else
	"(x86)"
#endif
	,
	0,	// hMainWindow (filled in by winamp)
	0,  // hDllInstance (filled in by winamp)
	"AMR\0AMR Audio File (*.AMR)\0"
	// this is a double-null limited list. "EXT\0Description\0EXT\0Description\0" etc.
	,
	1,	// is_seekable
	1,	// uses output plug-in system
	config,
	about,
	init,
	quit,
	getfileinfo,
	infoDlg,
	isourfile,
	play,
	pause,
	unpause,
	ispaused,
	stop,
	
	getlength,
	getoutputtime,
	setoutputtime,

	setvolume,
	setpan,

	0,0,0,0,0,0,0,0,0, // visualization calls filled in by winamp

	0,0, // dsp calls filled in by winamp

	eq_set,

	NULL,		// setinfo call filled in by winamp

	0 // out_mod filled in by winamp

};

// exported symbol. Returns output module.

__declspec( dllexport ) In_Module * winampGetInModule2()
{
	return &mod;
}
