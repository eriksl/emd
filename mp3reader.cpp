/* this file is derived from various files of "poc":
 *
 * Copyright (c) 2005, Manuel Odendahl, Florian Wesch
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 * Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 * 
 * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * 
 * The names of its contributors may not be used to endorse or promote
 * products derived from this software without specific prior written
 * permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <fcntl.h>

#include "mp3reader.h"
#include "syslog.h"

MP3Reader::MP3Reader(string filename) throw(string)
{
	if((fd = open(filename.c_str(), O_RDONLY, 0)) < 0)
		throw(string("MP3Reader::MP3Reader: cannot open file: " + filename));

	elapsed			= 0;
	raw_buffer		= 0;
	raw_buffer_size	= 0;

	buffer_reset();
}

MP3Reader::~MP3Reader()
{
	free(raw_buffer);

	close(fd);
}

/* get the b lower bits of byte x */

#define LOWERBITS(x, b) ((b) ? (x) & ((2 << ((b) - 1)) - 1) : 0)

/* get the b higher bits of byte x */

#define HIGHERBITS(x, b) (((x) & 0xff) >> (8 - (b)))

/*
 * initialize a bit vector structure bv with byte data data, and bit
 * length len. The internal index of the bit vector is set to 0, so that
 * future bv_get_bits or bv_put_bits will access the first
 * bit.
 */

void MP3Reader::bv_init(bv_t * bv, uint8_t *data, unsigned int len) const
{
	bv->data	= data;
	bv->len		= len;
	bv->idx		= 0;
}

/* get next bits of bit vector. returns the next numbits bits from bv */

unsigned long MP3Reader::bv_get_bits(bv_t * bv, unsigned int numbits) const
{
	unsigned int cidx = bv->idx >> 3;		/* char index */
	unsigned int overflow = bv->idx & 0x7;	/* bit overflow from previous char */

	bv->idx += numbits;

	/* most significant bit first */

	if(numbits <= (8 - overflow))
		return(HIGHERBITS(bv->data[cidx] << overflow, numbits));

	/* length in bytes of bitstring */

	unsigned int len = ((numbits + overflow) >> 3) + 1;

	/* number of bits of bitstring in first byte */

	unsigned long res = LOWERBITS(bv->data[cidx++], 8 - overflow);
	unsigned int i;

	for(i = 1; i < len - 1; i++)
		res = (res << 8) | (bv->data[cidx++] & 0xff);

	/* number of bits in last byte */

	unsigned int lastbits = (overflow + numbits) & 0x07;
	res = (res << lastbits) | HIGHERBITS(bv->data[cidx], lastbits);

	return(res);
}

/*
 *	calculate various information about the MP3 frame. 
 *	frame size and frame data size are given in
 *	bytes. frame data size is the size of the MP3 frame without
 *	header and without side information.
 */

void MP3Reader::mp3_calc_hdr()
{
	/*	bitrate table for MPEG Audio Layer III version 1.0, free format MP3s are considered invalid (for now) */
	static const int bitratetable[16] = { -1, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, -1 }; 
	/* samplerate table for MPEG Audio Layer III version 1.0. */
	static const int sampleratetable[4]	= { 44100, 48000, 32000, -1 };	

	frame.bitrate			= bitratetable[frame.bitrate_index];
	frame.samplerate		= sampleratetable[frame.samplerfindex];
	frame.samplelen			= 1152;
	frame.si_size			= frame.mode != (unsigned char)3 ? 32 : 17;
	frame.si_bitsize		= frame.si_size * 8;
	frame.frame_size		= 144000 * frame.bitrate;
	frame.frame_size		/= frame.samplerate;
	frame.frame_size		+= frame.padding_bit;
	frame.frame_data_size	= frame.frame_size - 4 - frame.si_size;

	if(frame.crc_protected == 0)
		frame.frame_data_size -= 2;

	frame.usec = frame.frame_size * 8 * 1000.0 / frame.bitrate;
}

int MP3Reader::mp3_read_si()
{
	uint8_t *ptr = frame.raw + 4; /* skip header */

	if(frame.crc_protected == 0)
		ptr += 2;

	bv_t bv;
	bv_init(&bv, ptr, frame.si_bitsize);

	frame.si.channel[0].granule[0].part2_3_length = 0;
	frame.si.channel[0].granule[1].part2_3_length = 0;
	frame.si.channel[1].granule[0].part2_3_length = 0;
	frame.si.channel[1].granule[1].part2_3_length = 0;

	mp3_si_t * si = &frame.si;
	unsigned int nch = (frame.mode != 3) ? 2 : 1;

	frame.adu_bitsize	= 0;
	si->main_data_end	= bv_get_bits(&bv, 9);
	si->private_bits	= nch == 2 ? bv_get_bits(&bv, 3) : bv_get_bits(&bv, 5);
	unsigned int i;

	for(i = 0; i < nch; i++)
	{
		unsigned int band;

		for(band = 0; band < 4; band++)
			si->channel[i].scfsi[band] = bv_get_bits(&bv, 1);
	}

	unsigned int gri;

	for(gri = 0; gri < 2; gri++)
	{
		for(i = 0; i < nch; i++)
		{
			mp3_granule_t * gr	= &si->channel[i].granule[gri];
			gr->part2_3_length	= bv_get_bits(&bv, 12);
			frame.adu_bitsize	+= gr->part2_3_length;
			gr->big_values		= bv_get_bits(&bv, 9);
			gr->global_gain		= bv_get_bits(&bv, 8);
			gr->scale_comp		= bv_get_bits(&bv, 4);
			gr->blocksplit_flag	= bv_get_bits(&bv, 1);

			if(gr->blocksplit_flag != 0)
			{
				gr->block_type = bv_get_bits(&bv, 2);

				if(gr->block_type == 0)
				{
					vlog("Frame has reserved windowing type, skipping...\n");
					return(0);
				}

				gr->switch_point	= bv_get_bits(&bv, 1);
				gr->tbl_sel[0]		= bv_get_bits(&bv, 5);
				gr->tbl_sel[1]		= bv_get_bits(&bv, 5);
				gr->tbl_sel[2]		= 0;

				unsigned int j;

				for (j = 0; j < 3; j++)
					gr->sub_gain[j] = bv_get_bits(&bv, 3);

				if (gr->block_type == 2)
					gr->reg0_cnt = 9;
				else
					gr->reg0_cnt = 8;

				gr->reg1_cnt = 0;
			}
			else
			{
				unsigned int j;

				for(j = 0; j < 3; j++)
					gr->tbl_sel[j] = bv_get_bits(&bv, 5);

				gr->reg0_cnt = bv_get_bits(&bv, 4);
				gr->reg1_cnt = bv_get_bits(&bv, 3);

				gr->block_type		= 0;
				gr->switch_point	= 0;
			}

			gr->preflag		= bv_get_bits(&bv, 1);
			gr->scale_scale	= bv_get_bits(&bv, 1);
			gr->cnt1tbl_sel = bv_get_bits(&bv, 1);

			static const int slen_table[2][16] =
			{
				{
					0, 0, 0, 0,
					3, 1, 1, 1,
					2, 2, 2, 3,
					3, 3, 4, 4
				},
				{
					0, 1, 2, 3,
					0, 1, 2, 3,
					1, 2, 3, 1,
					2, 3, 2, 3
				}
			};

			gr->slen0 = slen_table[0][gr->scale_comp];
			gr->slen1 = slen_table[1][gr->scale_comp];

			if (gr->block_type == 2)
			{
				if (gr->switch_point != 0)
					gr->part2_length = 17 * gr->slen0 + 18 * gr->slen1;
				else
					gr->part2_length = 18 * gr->slen0 + 18 * gr->slen1;
			}
			else
				gr->part2_length = 11 * gr->slen0 + 10 * gr->slen1;

			gr->part3_length = gr->part2_3_length - gr->part2_length;
		}
	}

	frame.adu_size = (frame.adu_bitsize + 7) / 8;

	return(1);
}

int MP3Reader::mp3_read_hdr()
{
	bv_t bv;
	bv_init(&bv, frame.raw, 4 * 8);

	if (bv_get_bits(&bv, 12) != 0xfff)
		return(0);

	frame.id = bv_get_bits(&bv, 1);

	if(frame.id == 0)
		return(0);

	frame.layer = bv_get_bits(&bv, 2);

	if(frame.layer != 1)
		return(0);

	frame.crc_protected		= bv_get_bits(&bv, 1);
	frame.bitrate_index		= bv_get_bits(&bv, 4);
	frame.samplerfindex		= bv_get_bits(&bv, 2);
	frame.padding_bit		= bv_get_bits(&bv, 1);
	frame.private_bit		= bv_get_bits(&bv, 1);
	frame.mode				= bv_get_bits(&bv, 2);
	frame.mode_ext			= bv_get_bits(&bv, 2);
	frame.copyright			= bv_get_bits(&bv, 1);
	frame.original			= bv_get_bits(&bv, 1);
	frame.emphasis			= bv_get_bits(&bv, 2);

	if(frame.crc_protected == 0)
	{
		frame.crc[0] = frame.raw[4];
		frame.crc[1] = frame.raw[5];
	}

	mp3_calc_hdr();

	return(1);
}

int MP3Reader::mp3_skip_id3v2()
{
	uint8_t id3v2_hdr[10];

	memcpy(id3v2_hdr, frame.raw, 4);

	if ((id3v2_hdr[0] != 'I') ||
			(id3v2_hdr[1] != 'D') ||
			(id3v2_hdr[2] != '3')) 
		return(0);

	//if(read(fd, id3v2_hdr + 4, 6) != 6)
		//return(-1);
	
	if(buffer_read(6, id3v2_hdr + 4) != 6)
		return(-1);

	size_t tag_size =
		(((id3v2_hdr[6] & 0x7F) << 21) |
		((id3v2_hdr[7] & 0x7F) << 14) |
		((id3v2_hdr[8] & 0x7F) << 7) |
		(id3v2_hdr[9] & 0x7F)) +
		(id3v2_hdr[5] & 0x10 ? 10 : 0);

	//if(lseek64(fd, tag_size, SEEK_CUR) == -1)
		//return(0);
	buffer_read(tag_size, 0);

	return(1);
}

int MP3Reader::mp3_next_frame()
{
	int resync = 0;

again:
	if(resync != 0)
	{
		frame.raw[0] = frame.raw[1];
		frame.raw[1] = frame.raw[2];
		frame.raw[2] = frame.raw[3];

		//if(read(fd, frame.raw + 3, 1) <= 0)
			//return(1);

		if(buffer_read(1, frame.raw + 3) <= 0)
			return(1);
	}
	else
	{
		//if(read(fd, frame.raw, MP3_HDR_SIZE) <= 0)
			//return(1);
		if(buffer_read(MP3_HDR_SIZE, frame.raw) <= 0)
			return(1);
	}

	if((frame.raw[0] == 0xff) && (((frame.raw[1] >> 4) & 0xf) == 0xf))
	{
		if(!mp3_read_hdr())
			goto resync;
		else 
			resync = 0;
	}
	else
	{
		if((frame.raw[0] == 'I') &&
				(frame.raw[1] == 'D') &&
				(frame.raw[2] == '3'))
		{
			if (!mp3_skip_id3v2())
				goto resync;
			else
			{
				resync = 0;
				goto again;
			}
		}
		else
			goto resync;
	}

	if(frame.frame_size > sizeof(frame.raw))
		goto resync;

	//if(read(fd, frame.raw + 4, frame.frame_size - 4) <= 0)
		//return(1);

	if(buffer_read(frame.frame_size - 4, frame.raw + 4) <= 0)
		return(1);

	if(!mp3_read_si())
		goto again;

	return(0);

resync:
	frame.syncskip++;

	if(resync++ > MP3_MAX_SYNC)
	{
		vlog("Max sync exceeded: %d\n", resync);
		return(1);
	}
	else
		goto again;
}

int MP3Reader::buffer_read(int size, uint8_t * buffer)
{
	int rv;

	if((raw_buffer_index + size + 2) > raw_buffer_size)
	{
		raw_buffer_size = raw_buffer_size + size + 2;
		raw_buffer = (uint8_t *)realloc(raw_buffer, raw_buffer_size);
	}

	rv = read(fd, raw_buffer + raw_buffer_index, size);

	if(rv > 0)
	{
		if(buffer)
			memcpy(buffer, raw_buffer + raw_buffer_index, rv);
		raw_buffer_index += rv;
	}

	return(rv);
}

void MP3Reader::buffer_reset()
{
	raw_buffer_index = 0;
}

unsigned int MP3Reader::get_frame(mp3reader_frame_t & frame_in)
{
	buffer_reset();

	if(mp3_next_frame())
		return(0);

	elapsed			+= frame.usec;
	frame_in.stamp	= elapsed;
	frame_in.length	= raw_buffer_index;
	frame_in.buffer	= raw_buffer;

	return(raw_buffer_index);
}

