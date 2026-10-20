// The XDK samples' CXBMemUnit as UnleashX links it (mounted with the
// internal XMountMURootA: the whole unit as a drive). Xbox only. See
// system/XBMemUnit.h.
#include "../system/XBMemUnit.h"

#include "../common/XboxKernel.h"

// 0x000981A8 (this in eax)
CXBMemUnit::CXBMemUnit(DWORD port, DWORD slot) : m_dwPort(port), m_dwSlot(slot), m_chDrive(0)
{
}

// 0x00098191 (this in eax)
CXBMemUnit::CXBMemUnit(const CXBMemUnit &other) : m_dwPort(other.m_dwPort), m_dwSlot(other.m_dwSlot), m_chDrive(0)
{
}

// 0x00098180
CXBMemUnit &CXBMemUnit::operator=(const CXBMemUnit &other)
{
	m_dwPort = other.m_dwPort;
	m_dwSlot = other.m_dwSlot;
	m_chDrive = 0;
	return *this;
}

// 0x00098255
CXBMemUnit::~CXBMemUnit()
{
	Unmount();
}

// 0x0009811F: the top slots are bits 0-3, the bottom ones bits 16-19.
DWORD CXBMemUnit::GetMask(DWORD index)
{
	DWORD mask = 1 << GetPort(index);
	if (GetSlot(index) == 1)
		mask <<= 16;
	return mask;
}

// 0x00098156
DWORD CXBMemUnit::GetDevices()
{
	return XGetDevices(XDEVICE_TYPE_MEMORY_UNIT);
}

// 0x00098143
BOOL CXBMemUnit::GetDeviceChanges(DWORD *insertions, DWORD *removals)
{
	return XGetDeviceChanges(XDEVICE_TYPE_MEMORY_UNIT, insertions, removals);
}

// 0x0009816D
BOOL CXBMemUnit::IsValid() const
{
	return m_dwPort != (DWORD)-1 && m_dwSlot != (DWORD)-1;
}

// 0x000981BF
BOOL CXBMemUnit::IsMounted() const
{
	return IsValid() && m_chDrive != 0;
}

// 0x000981F2
BOOL CXBMemUnit::Mount(DWORD *error)
{
	if (IsMounted())
		return TRUE;
	Unmount();
	char drive;
	*error = XMountMURootA(m_dwPort, m_dwSlot, &drive);
	if (*error != ERROR_SUCCESS)
		return FALSE;
	m_chDrive = drive;
	return TRUE;
}

// 0x000981D7
void CXBMemUnit::Unmount()
{
	if (!IsMounted())
		return;
	XUnmountMU(m_dwPort, m_dwSlot);
	m_chDrive = 0;
}

// 0x00098233
void CXBMemUnit::SetPortSlot(DWORD port, DWORD slot)
{
	if (m_dwPort == port && m_dwSlot == slot)
		return;
	Unmount();
	m_dwPort = port;
	m_dwSlot = slot;
}

// 0x000982F4
void CXBMemUnit::Clear()
{
	Unmount();
	m_dwPort = (DWORD)-1;
	m_dwSlot = (DWORD)-1;
}

// 0x00098265: "Xbox MU 1A", and " (name)" when mounted and named.
void CXBMemUnit::GetName(wchar_t *name)
{
	lstrcpyW(name, L"Xbox MU ");
	int length = lstrlenW(name);
	name[length] = (wchar_t)(L'1' + m_dwPort);
	name[length + 1] = (wchar_t)(L'A' + m_dwSlot);
	name[length + 2] = 0;
	wchar_t unitName[32];
	DWORD error;
	if (IsMounted())
		error = XMUNameFromDriveLetter(m_chDrive, unitName, 32);
	else
	{
		error = ERROR_SUCCESS;
		unitName[0] = 0;
	}
	if (error == ERROR_SUCCESS && unitName[0] != 0)
	{
		wchar_t suffix[36];
		wsprintfW(suffix, L" (%ls)", unitName);
		lstrcatW(name, suffix);
	}
}
