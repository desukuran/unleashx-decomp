#include "FtpEvents.h"

#include <cstring>

#include "../common/FileUtil.h"
#include "../common/StrUtil.h"
#include "../net/Network.h"
#include "../system/Drives.h"
#include "../system/Storage.h"
#include "../system/SystemControl.h"
#include "../system/SystemInfo.h"
#include "Screens.h"
#include "UnleashX.h"

CFTPEventReceiver g_ftpEventReceiver;
BYTE g_bFtpActive;
BOOL g_bFtpLaunchPending;
std::string g_strFtpLaunchPath;

// Any FTP activity also cancels a pending shutdown.
static void CancelShutdown()
{
	if (g_bShuttingDown)
	{
		g_bShuttingDown = FALSE;
		g_dwShutdownDeadline = 0;
	}
}

// 0x0002B9D0
void CFTPEventReceiver::OnStatChanged(int, DWORD value)
{
	g_bFtpActive = value > 0;
	CancelShutdown();
}

// "\C\dir\file" or "C\dir\file" (what FTP-style paths become) to
// "C:\dir\file".
static void AddDriveColon(std::string &path)
{
	if (StrFind(path, ":", 0) == -1)
	{
		std::string rest = StrMid(path, 1, path.size() - 1);
		std::string drive = StrLeft(path, 1);
		StrFormat(path, "%s:%s", drive.c_str(), rest.c_str());
	}
}

// 0x0002B9F4
// SITE commands. The text after SITE arrives with ':' removed and '\'
// turned into '/' (see CFTPConnection::Dispatch), which is why paths are
// rebuilt here. Returns the reply code; 0 = not implemented.
int CFTPEventReceiver::OnSiteCommand(const char *command, std::string *reply)
{
	std::string text = command;
	if (command == NULL || text.size() == 0)
	{
		CancelShutdown();
		return 0;
	}

	int code = 200;
	const char *message;
	if (StrCmpNoCase(text.c_str(), "REBOOT") == 0 || StrCmpNoCase(text.c_str(), "RESTART") == 0)
	{
		RebootToDashboard();
		return code;
	}
	if (StrCmpNoCase(text.c_str(), "SHUTDOWN") == 0)
	{
		*reply = "Initiating Shutdown";
		g_bShuttingDown = TRUE;
		g_dwShutdownDeadline = 10;
		return 200;
	}
	if (StrCmpNoCase(text.c_str(), "POWERCYCLE") == 0)
	{
		SmcPowerCycle();
		return code;
	}
	if (StrCmpNoCase(text.c_str(), "FTPRESET") == 0)
	{
		g_bFtpEnabled = FALSE;
		if (IsFtpServerRunning())
			StopFtpServer();
		Sleep(1000);
		if (IsFtpServerRunning())
			message = "FTP Server is already running";
		else if (StartFtpServer())
			message = "FTP Server is successfully started";
		else
			message = "FTP Server failed to start";
		*reply = message;
		g_bFtpEnabled = TRUE;
		return code;
	}
	if (StrCmpNoCase(text.c_str(), "FTPSTOP") == 0)
	{
		g_bFtpEnabled = FALSE;
		if (IsFtpServerRunning())
			StopFtpServer();
		Sleep(1000);
		return code;
	}
	if (StrCmpNoCase(text.c_str(), "NETRESET") == 0)
	{
		g_bFtpEnabled = FALSE;
		StopFtpServer();
		StopNetwork();
		Sleep(100);
		StartNetworkThread();
		*reply = "Network has been issued a restart command";
		g_bFtpEnabled = TRUE;
		return code;
	}
	if (StrCmpNoCase(text.c_str(), "TRAYCLOSE") == 0)
	{
		if (g_trayMonitor.CloseTray() < 0)
		{
			*reply = "Failed to close Tray";
			return 550;
		}
		*reply = "Tray was successfully closed";
		return code;
	}
	if (StrCmpNoCase(text.c_str(), "EJECT") == 0 || StrCmpNoCase(text.c_str(), "TRAYOPEN") == 0)
	{
		SmcSetResetOnEject(FALSE);
		if (g_trayMonitor.OpenTray() < 0)
		{
			*reply = "Failed to eject Tray";
			return 550;
		}
		*reply = "Tray was successfully ejected";
		return code;
	}
	if (StrCmpNoCase(text.c_str(), "DRIVESTAT") == 0)
	{
		// one line per mounted drive
		std::string line;
		for (int i = 0; i < DRIVE_INFO_COUNT; i++)
		{
			SDriveInfo *info = &g_drives[i];
			if (!info->mounted)
				continue;
			const double mb = 1.0 / 1048576;
			std::string total = FormatDouble(2, (double)(LONGLONG)info->totalBytes * mb);
			std::string free = FormatDouble(2, (double)(LONGLONG)info->freeBytes * mb);
			std::string used = FormatDouble(2, (double)(LONGLONG)info->usedBytes * mb);
			StrFormat(line, "%c\t Used: %sMB\tFree: %sMB\tTotal: %sMB\r", info->letter, used.c_str(), free.c_str(),
			          total.c_str());
			reply->append(line);
		}
		return code;
	}

	std::string part;
	if (StrCmpNoCase(StrLeft(text, 4), "EXEC") == 0)
	{
		// SITE EXEC <xbe>: launched by the main loop
		part = StrMid(text, 4, text.size() - 4);
		text = StrTrimRight(StrTrimLeft(part));
		StrReplaceAll(text, "/", "\\");
		StrReplaceAll(text, "\\\\", "\\");
		if (StrLeft(text, 1).compare("\\") == 0)
			text = StrMid(text, 1, text.size() - 1);
	}
	else if (StrCmpNoCase(StrLeft(text, 7), "LAUNCH ") == 0)
	{
		part = StrMid(text, 7, text.size() - 7);
		text = StrTrimRight(StrTrimLeft(part));
		StrReplaceAll(text, "/", "\\");
		StrReplaceAll(text, "\\\\", "\\");
	}
	else
	{
		if (text.compare("FREEROOTSPACEDISABLE") == 0)
		{
			*reply = " ";
			return code;
		}

		if (StrCmpNoCase(StrLeft(text, 4), "MD5 ") == 0)
		{
			// SITE MD5 <file>
			part = StrMid(text, 4, text.size() - 4);
			text = StrTrimRight(StrTrimLeft(part));
			StrReplaceAll(text, "/", "\\");
			StrReplaceAll(text, "\\\\", "\\");
			AddDriveColon(text);
			CMD5 md5(NULL, -1);
			if (!FileExists(text.c_str()) || !md5.HashFile(text.c_str()))
			{
				*reply = "File does not exist or MD5 calculation failed.";
				return 550;
			}
			StrFormat(*reply, "Command okay. MD5 is: %s.", md5.m_szHex);
			return 220;
		}

		if (StrLeft(text, 10).compare("XBERENAME ") != 0)
			return 0;

		// SITE XBERENAME "<xbe>" <new title>
		part = StrMid(text, 10, text.size() - 10);
		text = StrTrimRight(StrTrimLeft(part));
		StrReplaceAll(text, "/", "\\");
		StrReplaceAll(text, "\\\\", "\\");
		wchar_t title[41];
		memset(title, 0, sizeof(title));
		int quote = StrReverseFind(text, "\"", -1);
		if (quote == -1)
		{
			*reply = "XBERENAME Failed!";
			return 550;
		}
		part = StrMid(text, quote + 1, text.size() - (quote + 1));
		std::string name = StrTrimRight(StrTrimLeft(part));
		MultiByteToWideChar(CP_ACP, 0, name.c_str(), text.size() - quote, title, 0x28);
		text = StrLeft(text, quote);
		StrReplaceAll(text, "\"", "");
		AddDriveColon(text);
		if (SetXbeTitleName(text.c_str(), title))
		{
			*reply = "OK";
			return code;
		}
		*reply = "XBERENAME Failed!";
		return 550;
	}

	// EXEC and LAUNCH
	AddDriveColon(text);
	if (FileExists(text.c_str()))
	{
		g_strFtpLaunchPath = text;
		g_bFtpLaunchPending = TRUE;
		*reply = "EXEC command succeeded.";
		return code;
	}
	*reply = "EXEC command Failed. File does not exist.";
	return 550;
}

// 0x0002C44A
// EXEC <xbe>. Quirk: the server strips ':' from the argument and this
// rebuilds "C\dir\x.xbe" as "C:dir\x.xbe", without the backslash, so the
// file is never found; SITE EXEC works.
int CFTPEventReceiver::OnExec(const char *name, const char *)
{
	CancelShutdown();
	if (name == NULL || name[0] == '\0')
		return 0;

	std::string path = name;
	StrReplaceAll(path, "/", "\\");
	StrReplaceAll(path, "\\\\", "\\");
	if (StrLeft(path, 1).compare("\\") == 0)
		path = StrMid(path, 1, path.size() - 1);
	if (StrMid(path, 1, 1).compare("\\") == 0)
		path = StrLeft(path, 1) + ":" + StrMid(path, 2, path.size() - 2);
	if (!FileExists(path.c_str()))
		return 0;
	g_bFtpLaunchPending = TRUE;
	g_strFtpLaunchPath = path;
	return 200;
}
