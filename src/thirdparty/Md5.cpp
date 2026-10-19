// MD5 from RFC 1321 (see Md5.h).
#include "Md5.h"

#include <cstring>

static const unsigned int s_shifts[64] = {
	7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
	5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20,
	4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
	6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21,
};

// floor(abs(sin(i + 1)) * 2^32)
static const unsigned int s_constants[64] = {
	0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee, 0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
	0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be, 0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
	0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa, 0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
	0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed, 0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
	0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c, 0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
	0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05, 0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
	0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039, 0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
	0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1, 0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391,
};

static void Transform(unsigned int state[4], const unsigned char block[64])
{
	unsigned int x[16];
	for (int i = 0; i < 16; i++)
		x[i] = (unsigned int)block[i * 4] | ((unsigned int)block[i * 4 + 1] << 8) |
		       ((unsigned int)block[i * 4 + 2] << 16) | ((unsigned int)block[i * 4 + 3] << 24);
	unsigned int a = state[0], b = state[1], c = state[2], d = state[3];
	for (int i = 0; i < 64; i++)
	{
		unsigned int f;
		int g;
		if (i < 16)
		{
			f = (b & c) | (~b & d);
			g = i;
		}
		else if (i < 32)
		{
			f = (d & b) | (~d & c);
			g = (5 * i + 1) % 16;
		}
		else if (i < 48)
		{
			f = b ^ c ^ d;
			g = (3 * i + 5) % 16;
		}
		else
		{
			f = c ^ (b | ~d);
			g = (7 * i) % 16;
		}
		unsigned int sum = a + f + s_constants[i] + x[g];
		a = d;
		d = c;
		c = b;
		b = b + ((sum << s_shifts[i]) | (sum >> (32 - s_shifts[i])));
	}
	state[0] += a;
	state[1] += b;
	state[2] += c;
	state[3] += d;
}

void MD5Init(MD5_CTX *context)
{
	context->count[0] = context->count[1] = 0;
	context->state[0] = 0x67452301;
	context->state[1] = 0xefcdab89;
	context->state[2] = 0x98badcfe;
	context->state[3] = 0x10325476;
}

void MD5Update(MD5_CTX *context, const unsigned char *input, unsigned int length)
{
	unsigned int index = (context->count[0] >> 3) & 0x3F;
	if ((context->count[0] += length << 3) < (length << 3))
		context->count[1]++;
	context->count[1] += length >> 29;
	unsigned int partLength = 64 - index;
	unsigned int i = 0;
	if (length >= partLength)
	{
		memcpy(&context->buffer[index], input, partLength);
		Transform(context->state, context->buffer);
		for (i = partLength; i + 63 < length; i += 64)
			Transform(context->state, &input[i]);
		index = 0;
	}
	memcpy(&context->buffer[index], &input[i], length - i);
}

void MD5Final(unsigned char digest[16], MD5_CTX *context)
{
	unsigned char bits[8];
	for (int i = 0; i < 8; i++)
		bits[i] = (unsigned char)(context->count[i / 4] >> ((i % 4) * 8));
	static const unsigned char padding[64] = { 0x80 };
	unsigned int index = (context->count[0] >> 3) & 0x3F;
	unsigned int padLength = index < 56 ? 56 - index : 120 - index;
	MD5Update(context, padding, padLength);
	MD5Update(context, bits, 8);
	for (int i = 0; i < 16; i++)
		digest[i] = (unsigned char)(context->state[i / 4] >> ((i % 4) * 8));
	memset(context, 0, sizeof(*context));
}
