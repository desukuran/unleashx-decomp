// MD5 (RFC 1321). The original links RSA Data Security's reference code
// (MD5Init 0x000B1B6F, MD5Update 0x000B1AD8, MD5Final 0x000B1B93, called
// with the context in eax/edi); Md5.cpp is an equivalent implementation
// written from the RFC.
#pragma once

struct MD5_CTX
{
	unsigned int state[4];          // A, B, C, D
	unsigned int count[2];          // bits, low then high
	unsigned char buffer[64];
};

void MD5Init(MD5_CTX *context);
void MD5Update(MD5_CTX *context, const unsigned char *input, unsigned int length);
void MD5Final(unsigned char digest[16], MD5_CTX *context);
