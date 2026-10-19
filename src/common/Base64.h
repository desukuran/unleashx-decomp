// Base64, as the FTP manager stores passwords in FtpSrv.xml. In the
// original it's a small class (buffers, lengths, the decoding table at
// +0x1C) used as a temporary; these are its two operations.
#pragma once

#include <string>

#include "Platform.h"

// FALSE for NULL. (this in ecx)
BOOL Base64Encode(const char *text, std::string &out);   // 0x0001E977 (-> 0x0001EAE4)
// Skips tabs, line breaks and spaces. Letters outside the alphabet aren't
// rejected (their table value 0xFE goes into the bits). FALSE for NULL.
// (this in eax)
BOOL Base64Decode(const char *text, std::string &out);   // 0x0001E9B4 (-> 0x0001E9F0)
