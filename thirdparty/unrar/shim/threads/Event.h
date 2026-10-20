// UnleashX build shim: XBMC's CEvent as UnrarXLib uses it. The extraction
// code (urarlib_get) never creates its events and still calls hQuit and
// friends in places (unpack15.cpp), so a NULL event reads as "not set".
// (Calling through NULL works with MSVC as the methods check this first.)
// Not XBMC code.
#pragma once
#include <xtl.h>

class CEvent
{
public:
	CEvent(bool manualReset = true) { m_hEvent = CreateEventA(NULL, manualReset, FALSE, NULL); }
	~CEvent() { CloseHandle(m_hEvent); }
	void Set()
	{
		if (this != NULL)
			SetEvent(m_hEvent);
	}
	void Reset()
	{
		if (this != NULL)
			ResetEvent(m_hEvent);
	}
	bool WaitMSec(unsigned int milliseconds)
	{
		return this != NULL && WaitForSingleObject(m_hEvent, milliseconds) == WAIT_OBJECT_0;
	}

private:
	HANDLE m_hEvent;
};
