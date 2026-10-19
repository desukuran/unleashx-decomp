// Entry point.
#include <cstdio>
#include <cstring>

#include "../common/FileUtil.h"
#include "../common/XboxKernel.h"
#include "../system/SystemControl.h"
#include "UnleashX.h"

BOOL g_bColdBoot;                         // 0x00276208

// 0x000E1EBB
// Unhandled exceptions: write the exception record to Z:\debug.txt and
// reboot.
static LONG __stdcall CrashHandler(EXCEPTION_POINTERS *exception)
{
	EXCEPTION_RECORD record = *exception->ExceptionRecord;
	char text[100];
	memset(text, 0, sizeof(text));
	sprintf(text, "ExceptionCode: %08x \r\nExceptionAddress: %08x\r\nExceptionFlags: %d\r\nNumberParameters: %d",
	        (unsigned)record.ExceptionCode, (unsigned)(uintptr_t)record.ExceptionAddress,
	        (int)record.ExceptionFlags, (int)record.NumberParameters);
	WriteTextFile("Z:\\debug.txt", text, FALSE);
	HalReturnToFirmware(ReturnFirmwareQuickReboot);
	return 0;   // not reached
}

// 0x000E24D4
int main()
{
	SetUnhandledExceptionFilter(CrashHandler);

	// A clock outside 2007-2099 means it was lost (cold boot without the
	// clock capacitor): set it to the build date, 2007-05-28 08:00, and ask
	// for the time later.
	SYSTEMTIME st;
	GetLocalTime(&st);
	if (st.wYear < 2007 || st.wYear > 2099)
	{
		g_bColdBoot = TRUE;
		st.wDay = 28;
		st.wMonth = 5;
		st.wYear = 2007;
		st.wHour = 8;
		st.wMinute = 0;
		st.wSecond = 0;
		FILETIME ft;
		SystemTimeToFileTime(&st, &ft);
		LocalFileTimeToFileTime(&ft, &ft);
		LARGE_INTEGER previous;
		KeQuerySystemTime(&previous);
		NtSetSystemTime((LARGE_INTEGER *)&ft, &previous);
		GetSystemTime(&st);
		if (st.wYear > 2099)
		{
			KeQuerySystemTime(&previous);
			NtSetSystemTime((LARGE_INTEGER *)&ft, &previous);
		}
	}

	// The application object lives on main's stack (0x2268 bytes).
	CUnleashX app;
	g_pApp = &app;
	if (FAILED(app.Create()))
		SmcPowerCycle();
	app.Run();
	// Run() only returns when the dashboard is done.
	SmcPowerCycle();
	return 0;
}
