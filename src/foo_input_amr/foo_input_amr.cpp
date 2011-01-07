// oAMRpF - AMR Plugin for foobar2000
// Using GPL v2
// Author: http://otnth.blogspot.com

#include "foobar2000/SDK/foobar2000.h"

extern "C" {
#include "c-code/interf_dec.h"
#include "c-code/sp_dec.h"
#include "c-code/typedef.h"
}

#define AMR_MAGIC_NUMBER "#!AMR\n"

enum {
	amr_bits_per_sample = 16,
	amr_channels = 1,
	amr_sample_rate = 8000,

	amr_bytes_per_sample = amr_bits_per_sample / 8,
	amr_total_sample_width = amr_bytes_per_sample * amr_channels,

	amr_samples_per_frame = 160,
	amr_frame_max_width = 32,
};

// No inheritance. Our methods get called over input framework templates. See input_singletrack_impl for descriptions of what each method does.
class input_amr {
public:
	void open(service_ptr_t<file> p_filehint,const char * p_path,t_input_open_reason p_reason,abort_callback & p_abort) {
		if (p_reason == input_open_info_write) throw exception_io_unsupported_format();//our input does not support retagging.
		m_file = p_filehint;//p_filehint may be null, hence next line
		input_open_file_helper(m_file,p_path,p_reason,p_abort);//if m_file is null, opens file with appropriate privileges for our operation (read/write for writing tags, read-only otherwise).

		// frame总数
		m_frame_count = _seek_file(-1, p_abort);
	}

	void get_info(file_info & p_info,abort_callback & p_abort) {
		// 每frame包含160次采样的数据
		p_info.set_length(audio_math::samples_to_time( m_frame_count*amr_samples_per_frame, amr_sample_rate));
		
		//note that the values below should be based on contents of the file itself, NOT on user-configurable variables for an example. To report info that changes independently from file contents, use get_dynamic_info/get_dynamic_info_track instead.
	
		p_info.info_set_int("samplerate",amr_sample_rate);
		p_info.info_set_int("channels",amr_channels);
		p_info.info_set_int("bitspersample",amr_bits_per_sample);
		p_info.info_set("encoding","lossless");
		p_info.info_set_bitrate((amr_bits_per_sample * amr_channels * amr_sample_rate + 500 /* rounding for bps to kbps*/ ) / 1000 /* bps to kbps */);
		
	}
	t_filestats get_file_stats(abort_callback & p_abort) {return m_file->get_stats(p_abort);}

	void decode_initialize(unsigned p_flags,abort_callback & p_abort) {
		m_file->reopen(p_abort);//equivalent to seek to zero, except it also works on nonseekable streams
		m_file->seek_ex(uCharLength(AMR_MAGIC_NUMBER), file::seek_from_beginning, p_abort);
		m_decoder_state = (int*)Decoder_Interface_init();
	}
	bool decode_run(audio_chunk & p_chunk,abort_callback & p_abort) {
		pfc::array_t<t_uint8> analysis;
		int l;

		m_buffer.set_size(amr_total_sample_width*amr_samples_per_frame);
		analysis.set_size(amr_frame_max_width);
		
		l = m_file->read(analysis.get_ptr(), 1, p_abort);
		if (l <= 0) {
			Decoder_Interface_exit(m_decoder_state);
			return false;
		}

		l = (analysis.get_ptr()[0] >> 3) & 0x000F;
		m_file->read(analysis.get_ptr()+1, m_block_size[l], p_abort);
		Decoder_Interface_Decode(m_decoder_state, analysis.get_ptr(), (short*)m_buffer.get_ptr(), 0);
		
		p_chunk.set_data_fixedpoint(m_buffer.get_ptr(), amr_total_sample_width*amr_samples_per_frame, amr_sample_rate, amr_channels, amr_bits_per_sample, audio_chunk::g_guess_channel_config(amr_channels));
		
		//processed successfully, no EOF
		return true;
	}
	void decode_seek(double p_seconds,abort_callback & p_abort) {
		m_file->ensure_seekable();//throw exceptions if someone called decode_seek() despite of our input having reported itself as nonseekable.
		// IMPORTANT: convert time to sample offset with proper rounding! audio_math::time_to_samples does this properly for you.
		int target = audio_math::time_to_samples(p_seconds, amr_sample_rate) / amr_samples_per_frame;
		
		// get_size_ex fails (throws exceptions) if size is not known (where get_size would return filesize_invalid). Should never fail on seekable streams (if it does it's not our problem anymore).
		if (target > m_frame_count) target = m_frame_count;//clip seek-past-eof attempts to legal range (next decode_run() call will just signal EOF).
		_seek_file(target, p_abort);
	}
	bool decode_can_seek() {return m_file->can_seek();}
	bool decode_get_dynamic_info(file_info & p_out, double & p_timestamp_delta) {return false;}
	bool decode_get_dynamic_info_track(file_info & p_out, double & p_timestamp_delta) {return false;}
	void decode_on_idle(abort_callback & p_abort) {m_file->on_idle(p_abort);}

	void retag(const file_info & p_info,abort_callback & p_abort) {throw exception_io_unsupported_format();}
	
	static bool g_is_our_content_type(const char * p_content_type) {
		popup_message::g_show(p_content_type, "tt");
		return false;
	}
	static bool g_is_our_path(const char * p_path,const char * p_extension) {
		return stricmp_utf8(p_extension,"amr") == 0;
	}

private:
	// 文件定位，dest为目标frame序数
	// 为-1时返回文件内的frame总数
	int _seek_file(int dest, abort_callback & p_abort) {
		unsigned char c;
		int l, n = 0;
		
		m_file->seek_ex(uCharLength(AMR_MAGIC_NUMBER), file::seek_from_beginning, p_abort);
		while (m_file->read(&c, 1, p_abort) == 1) {
			l = m_block_size[(c >> 3) & 0x000F];
			m_file->seek_ex(l, file::seek_from_current, p_abort);
			n++;
			
			if (n == dest) break;
		}
		return n;
	}

	
public:
	service_ptr_t<file> m_file;
	pfc::array_t<t_uint8> m_buffer;

private:
	int *m_decoder_state;
	int m_frame_count;		// sum of frames

	static const int m_block_size[16];
};

const int input_amr::m_block_size[16] = {12, 13, 15, 17, 19, 20, 26, 31, 5, 0, 0, 0, 0, 0, 0, 0};

static input_singletrack_factory_t<input_amr> g_input_amr_factory;


DECLARE_COMPONENT_VERSION("AMR input","0.1","oTnTh AMR Plugin, http://otnth.blogspot.com");
DECLARE_FILE_TYPE("AMR files","*.AMR");