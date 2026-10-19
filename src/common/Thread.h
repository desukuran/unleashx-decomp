// Thread: a small worker-thread base class (RTTI "Thread"). Derive, override
// Run, Start it; Run polls m_bTerminate to know when to stop. 0x10 bytes.
#pragma once

#include "Platform.h"

class Thread
{
public:
	Thread();                                   // 0x000AA458 (this in eax)
	// Not virtual: slot 0 is Run. Closes the handle. (this in eax)
	~Thread();                                  // 0x000AA448

	virtual unsigned Run() { return 0; }        // slot 0, 0x000AA3DC
	virtual void OnExit() {}                    // slot 1, 0x000AA3DB

	// Creates the thread (once; later calls return the same handle).
	// (this in esi, suspended in al)
	HANDLE Start(unsigned stackSize, bool suspended);   // 0x000AA416
	// Clears m_bTerminate and resumes it. (this in eax)
	DWORD Resume();                             // 0x000AA385
	DWORD Suspend();                            // 0x000AA39A (this in eax)
	// Waits for it to end and closes the handle. (this in eax)
	void Join();                                // 0x000AA46A
	// Asks Run to return (m_bTerminate) and joins it. (this in eax)
	void Stop();                                // 0x000234B4
	void CloseThread();                         // 0x000AA404 (this in esi)

	unsigned m_dwThreadId;                      // +0x04
	HANDLE m_hThread;                           // +0x08
	bool m_bTerminate;                          // +0x0C

private:
	static unsigned __stdcall ThreadProc(void *thread);   // 0x000AA3B8
};
