// libmad (Underbit's MPEG audio decoder, GPL), statically linked at
// 0x00058AD0-0x00059D90. Not decompiled: only what CAudioDecoder uses is
// declared here, with libmad 0.15's layouts (they match the offsets
// CAudioDecoder embeds them at).
#pragma once

#include "../common/Platform.h"

typedef int mad_fixed_t;
#define MAD_F_FRACBITS 28
#define MAD_BUFFER_GUARD 8
#define MAD_ERROR_BUFLEN 0x0001
#define MAD_RECOVERABLE(error) ((error) & 0xFF00)
#define MAD_MODE_SINGLE_CHANNEL 0
#define MAD_UNITS_SECONDS 0

struct mad_bitptr
{
	const unsigned char *byte;
	unsigned short cache;
	unsigned short left;
};

// 0x40 bytes.
struct mad_stream
{
	const unsigned char *buffer;      // +0x00
	const unsigned char *bufend;      // +0x04
	unsigned long skiplen;            // +0x08
	int sync;                         // +0x0C
	unsigned long freerate;           // +0x10
	const unsigned char *this_frame;  // +0x14
	const unsigned char *next_frame;  // +0x18
	mad_bitptr ptr;                   // +0x1C
	mad_bitptr anc_ptr;               // +0x24
	unsigned int anc_bitlen;          // +0x2C
	void *main_data;                  // +0x30
	unsigned int md_len;              // +0x34
	int options;                      // +0x38
	int error;                        // +0x3C
};

struct mad_timer_t
{
	long seconds;
	unsigned long fraction;
};
extern "C" const mad_timer_t mad_timer_zero;   // 0x002373E0

// 0x2C bytes.
struct mad_header
{
	int layer;                        // +0x00
	int mode;                         // +0x04
	int mode_extension;               // +0x08
	int emphasis;                     // +0x0C
	unsigned long bitrate;            // +0x10
	unsigned int samplerate;          // +0x14
	unsigned short crc_check;         // +0x18
	unsigned short crc_target;        // +0x1A
	int flags;                        // +0x1C
	int private_bits;                 // +0x20
	mad_timer_t duration;             // +0x24
};

// 0x2434 bytes.
struct mad_frame
{
	mad_header header;                // +0x0000
	int options;                      // +0x002C
	mad_fixed_t sbsample[2][36][32];  // +0x0030
	void *overlap;                    // +0x2430
};

struct mad_pcm
{
	unsigned int samplerate;          // +0x00
	unsigned short channels;          // +0x04
	unsigned short length;            // +0x06
	mad_fixed_t samples[2][1152];     // +0x08
};

// 0x340C bytes.
struct mad_synth
{
	mad_fixed_t filter[2][2][2][16][8];   // +0x0000
	unsigned int phase;               // +0x1000
	mad_pcm pcm;                      // +0x1004
};

extern "C" {
void mad_stream_init(mad_stream *stream);                                  // 0x00059040
void mad_stream_finish(mad_stream *stream);                                // 0x00059090
void mad_stream_buffer(mad_stream *stream, const unsigned char *buffer, unsigned long length);   // 0x000590B0
void mad_frame_init(mad_frame *frame);                                     // 0x00059830
void mad_frame_finish(mad_frame *frame);                                   // 0x00059140
int mad_frame_decode(mad_frame *frame, mad_stream *stream);                // 0x00059720
void mad_synth_init(mad_synth *synth);                                     // 0x00058B70
void mad_synth_frame(mad_synth *synth, const mad_frame *frame);            // 0x00058AD0
void mad_timer_add(mad_timer_t *timer, mad_timer_t incr);                  // 0x00058DA0
void mad_timer_set(mad_timer_t *timer, unsigned long seconds, unsigned long numer, unsigned long denom);   // 0x00058C70
void mad_timer_multiply(mad_timer_t *timer, signed long scalar);           // 0x00058DE0
signed long mad_timer_count(mad_timer_t timer, int units);                 // 0x00058E90
unsigned long mad_bit_read(mad_bitptr *bitptr, unsigned int len);          // 0x00059920
}
