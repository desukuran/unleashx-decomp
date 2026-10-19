// The built-in FTP server. TODO: decompile
#pragma once

#include "../common/Platform.h"

extern BYTE g_bFtpEnabled;            // 0x0026A62E the server thread may run
BOOL IsFtpServerRunning();            // 0x000B39A8
void StopFtpServer();                 // 0x000B3C5D
HRESULT StartFtpServer();             // 0x000BB22A
