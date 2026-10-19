// The dashboard's side of the FTP server: CFTPEventReceiver gets the
// server's statistics and runs the SITE and EXEC commands the server passes
// on. One global instance (0x0026C238), installed with
// CFTPServer::SetEventSink() by the network thread.
#pragma once

#include <string>

#include "../net/FtpServer.h"

// vtable 0x00254590
class CFTPEventReceiver : public CFTPEventSink
{
public:
	void OnStatChanged(int stat, DWORD value);                     // 0x0002B9D0
	int OnSiteCommand(const char *command, std::string *reply);   // 0x0002B9F4
	int OnExec(const char *name, const char *args);               // 0x0002C44A
};

extern CFTPEventReceiver g_ftpEventReceiver;     // 0x0026C238

// Set while an FTP value changed to something non-zero (users, transfers).
extern BYTE g_bFtpActive;                        // 0x00276234
// An XBE to launch, requested over FTP; the main loop launches it.
extern BOOL g_bFtpLaunchPending;                 // 0x00276214
extern std::string g_strFtpLaunchPath;           // 0x0026C21C
