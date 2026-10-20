// The XDK samples' CXBMemUnit: one memory unit slot (a port and a top or
// bottom slot) and the drive letter XAPI mounted it on. Library code: only
// declared. The methods are LTCG copies with register arguments (noted).
#pragma once

#include <vector>

#include "../common/Platform.h"

class CXBMemUnit
{
public:
	// (this in eax)
	CXBMemUnit(DWORD port = (DWORD)-1, DWORD slot = (DWORD)-1);   // 0x000981A8
	CXBMemUnit(const CXBMemUnit &other);                         // 0x00098191 (this in eax)
	virtual ~CXBMemUnit();                                       // 0x00098255
	// Copies the port and slot, not the mount. (this in eax)
	CXBMemUnit &operator=(const CXBMemUnit &other);              // 0x00098180

	// Units 0..7 are port * 2 + slot. (index in eax)
	static DWORD GetPort(DWORD index) { return index >> 1; }     // 0x0009811C
	static DWORD GetSlot(DWORD index) { return index & 1; }      // 0x00098118
	// The unit's bit in XGetDevices' mask.
	static DWORD GetMask(DWORD index);                           // 0x0009811F
	static DWORD GetDevices();                                   // 0x00098156
	static BOOL GetDeviceChanges(DWORD *insertions, DWORD *removals);   // 0x00098143

	char GetDrive() const { return m_chDrive; }                  // 0x00098169 (this in eax)
	BOOL IsValid() const;                                        // 0x0009816D (this in eax)
	BOOL IsMounted() const;                                      // 0x000981BF (this in ecx)
	// (this in eax) FALSE with *error set when XMountMURootA fails.
	BOOL Mount(DWORD *error);                                    // 0x000981F2
	void Unmount();                                              // 0x000981D7 (this in esi)
	// Unmounts when the port or slot changes. (this in eax, port in edi)
	void SetPortSlot(DWORD port, DWORD slot);                    // 0x00098233
	// Unmounts and forgets the slot. (this in eax)
	void Clear();                                                // 0x000982F4
	// "Xbox MU <port><slot>" and " (<name>)" when mounted and named. (this
	// in esi, name in edi)
	void GetName(wchar_t *name);                                 // 0x00098265

	DWORD m_dwPort;                // +0x04
	DWORD m_dwSlot;                // +0x08
	char m_chDrive;                // +0x0C 0 = not mounted
};

// One per memory unit slot, H..O (sized to 8 by MountAllDrives).
extern std::vector<CXBMemUnit> g_memoryUnits;   // 0x00278738
