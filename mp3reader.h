#ifndef mp3reader_h_
#define mp3reader_h_

#include <stdint.h>

#include "string"
using std::string;

class MP3Reader
{
	private:

		static const int MP3_MAX_SYNC	= 4000;	/* maximal number of synchronisation errors allowed while reading an MP3 stream */
		static const int MP3_HDR_SIZE	= 4;	/* MPEG Audio Frame Header size */

		typedef struct bv_s
		{
			uint8_t *		data;
			unsigned int	len;
			unsigned int	idx;
		} bv_t;

		typedef struct
		{
			unsigned int l[23];		/* long window */
			unsigned int s[3][13];	/* short window */
		} mp3_sf_t;

		typedef struct
		{
			int		s;
			float	x;
		} mp3_sample_t;

		typedef struct mp3_granule_s
		{
			int				part2_3_length;
			int				part2_length;
			int				part3_length;

			unsigned int	big_values;
			unsigned int	global_gain;

			unsigned int	scale_comp;
			unsigned int	slen0;
			unsigned int	slen1;

			unsigned int	blocksplit_flag;
			unsigned int	block_type;
			unsigned int	switch_point;
			unsigned int	tbl_sel[3];
			unsigned int	reg0_cnt;
			unsigned int	reg1_cnt;
			unsigned int	sub_gain[3];
			unsigned int	maxband[3];
			unsigned int	maxbandl;
			unsigned int	maxb;
			unsigned int	region1start;
			unsigned int	region2start;
			unsigned int	preflag;
			unsigned int	scale_scale;
			unsigned int	cnt1tbl_sel;
			float *			full_gain[3];
			float *			pow2gain;

			mp3_sf_t		sf;
			mp3_sample_t	samples[576];
		} mp3_granule_t;

		typedef struct mp3_channel_s
		{
			unsigned int scfsi[4];
			mp3_granule_t granule[2];
		} mp3_channel_t;

		typedef struct mp3_si_s
		{
			unsigned int	main_data_end;
			unsigned int	private_bits;

			mp3_channel_t	channel[2];
		} mp3_si_t;

		typedef struct mp3_frame_s
		{
			unsigned int	id;
			unsigned int	layer;
			unsigned int	crc_protected;
			unsigned int	bitrate_index;
			unsigned int	samplerfindex;
			unsigned int	padding_bit;
			unsigned int	private_bit; 
			unsigned int	mode;
			unsigned int	mode_ext;
			unsigned int	copyright;
			unsigned int	original;
			unsigned int	emphasis;

			unsigned int	crc[2];
 
			mp3_si_t		si;
			unsigned long	si_size;
			unsigned long	si_bitsize;
			unsigned long	adu_bitsize;
			unsigned long	adu_size;
		
			unsigned short	syncskip;
			unsigned long	bitrate;
			unsigned long	samplerate;
			unsigned long	samplelen;
			unsigned long	frame_size;
			unsigned long	frame_data_size;
			uint64_t		usec;
			uint8_t			raw[4096];
		} mp3_frame_t;

		int				fd;
		mp3_frame_t		frame;
		uint64_t		elapsed;
		uint8_t *		raw_buffer;
		int				raw_buffer_index;
		int				raw_buffer_size;

		void			bv_init(bv_t * bv, uint8_t * data, unsigned int len) const;
		unsigned long	bv_get_bits(bv_t * bv, unsigned int numbits) const;

		void			mp3_calc_hdr();
		int				mp3_read_si();
		int				mp3_read_hdr();
		int				mp3_skip_id3v2();
		int				mp3_next_frame();

		int				buffer_read(int size, uint8_t * buffer); 
		void			buffer_reset();

	public:

						typedef struct
						{
							uint64_t	stamp;
							int			length;
							uint8_t *	buffer;
						} mp3reader_frame_t;

						MP3Reader(string filename) throw(string);
						~MP3Reader();

		unsigned int	get_frame(mp3reader_frame_t & frame);
};

#endif
