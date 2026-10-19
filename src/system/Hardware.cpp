// Hardware readers of SystemInfo.h: the SMC over SMBus, ATA IDENTIFY and
// SMART through IDE pass-through, and the BIOS flash chip ID. Xbox only; the
// host test build uses stand-ins.
#include "SystemInfo.h"

#include <cstdio>
#include <cstring>

#include "../common/StrUtil.h"
#include "../common/XboxKernel.h"
#include "../thirdparty/XKUtils.h"

// SMBus addresses
#define SMBUS_SMC 0x20
#define SMBUS_FOCUS 0xD4   // video encoder of the v1.4 boards

// SMC registers
#define SMC_VERSION 0x01
#define SMC_CPU_TEMP 0x09
#define SMC_MB_TEMP 0x0A

// 0x0008F7CB
// out must be zeroed by the caller; the "V1.x" strings are copied without
// their terminator.
void GetXboxVersion(char *out)
{
	// The SMC returns its 3-character version one byte per read.
	char version[4] = {0, 0, 0, 0};
	for (int i = 0; i < 3; i++)
	{
		ULONG value = 0;
		HalReadSMBusValue(SMBUS_SMC, SMC_VERSION, FALSE, &value);
		version[i] = (char)value;
		Sleep(10);
	}
	version[3] = 0;

	if (memcmp(version, "P01", 4) == 0)
		memcpy(out, "V1.0", 4);
	else if (memcmp(version, "P05", 4) == 0)
		memcpy(out, "V1.1", 4);
	else if (memcmp(version, "P11", 4) == 0)
	{
		// 1.2/1.3 and 1.4 share the SMC; 1.4 has a Focus encoder.
		ULONG value;
		memcpy(out, HalReadSMBusValue(SMBUS_FOCUS, 0, FALSE, &value) == 0 ? "V1.4" : "V1.2", 4);
	}
	else if (memcmp(version, "P2L", 4) == 0)
		memcpy(out, "V1.6", 4);
	else if (memcmp(version, "DBG", 4) == 0)
		strcpy(out, "DBG");
	else if (memcmp(version, "01D", 4) == 0 || memcmp(version, "D01", 4) == 0 ||
	         memcmp(version, "1D0", 4) == 0 || memcmp(version, "0D1", 4) == 0)
		strcpy(out, "Devkit");
}

// 0x0009E69A
float ReadTemperature(BOOL cpu, BOOL celsius)
{
	ULONG value;
	Sleep(10);
	HalReadSMBusValue(SMBUS_SMC, cpu ? SMC_CPU_TEMP : SMC_MB_TEMP, FALSE, &value);
	float temp = (float)(int)value;
	if (!celsius)
		temp = temp * 1.8f + 32.5f;   // sic: 32.5
	return temp;
}

// --- ATA --------------------------------------------------------------------

#define ATA_IDENTIFY_DEVICE 0xEC
#define ATA_IDENTIFY_PACKET_DEVICE 0xA1
#define ATA_SMART 0xB0
#define SMART_READ_DATA 0xD0
#define SMART_ENABLE_OPERATIONS 0xD8
#define SMART_CYL_LOW 0x4F
#define SMART_CYL_HIGH 0xC2

static BYTE s_hddIdentify[0x200];        // 0x00272230
static BYTE s_dvdIdentify[0x200];        // 0x00272430

// ATA strings are byte-swapped per word and padded with spaces.
static void FixAtaString(BYTE *s, int words)
{
	for (int i = 0; i < words; i++)
	{
		BYTE b = s[i * 2];
		s[i * 2] = s[i * 2 + 1];
		s[i * 2 + 1] = b;
	}
	// Trailing spaces become NULs. Quirk: the check starts one before the
	// last character.
	for (int i = words * 2 - 2; i >= 0 && s[i] == ' '; i--)
		s[i] = 0;
}

static HANDLE OpenIdeDevice(const char *device, HANDLE *handle)
{
	ANSI_STRING name;
	RtlInitAnsiString(&name, device);
	OBJECT_ATTRIBUTES attributes = {NULL, &name, OBJ_CASE_INSENSITIVE};
	IO_STATUS_BLOCK status;
	NtCreateFile(handle, GENERIC_READ | SYNCHRONIZE, &attributes, &status, NULL, FILE_ATTRIBUTE_NORMAL,
	             FILE_SHARE_READ | FILE_SHARE_WRITE, FILE_OPEN, FILE_SYNCHRONOUS_IO_NONALERT);
	return *handle;
}

// 0x0009029C
// Errors aren't checked: on failure the buffer gets whatever the pass-through
// buffer held.
static void IdentifyDevice(BYTE *identify, BOOL hdd)
{
	memset(identify, 0, 0x200);
	HANDLE device;
	OpenIdeDevice(hdd ? "\\Device\\Harddisk0\\Partition0" : "\\Device\\CdRom0", &device);

	BYTE data[0x200];
	ATA_PASS_THROUGH ata;
	memset(&ata.IdeReg, 0, sizeof(ata.IdeReg));
	ata.IdeReg.bCommandReg = hdd ? ATA_IDENTIFY_DEVICE : ATA_IDENTIFY_PACKET_DEVICE;
	ata.DataBufferSize = sizeof(data);
	ata.DataBuffer = data;
	DWORD returned;
	DeviceIoControl(device, IOCTL_IDE_PASS_THROUGH, &ata, sizeof(ata), &ata, sizeof(ata), &returned, NULL);
	NtClose(device);
	memcpy(identify, ata.DataBuffer, 0x200);

	FixAtaString(identify + 0x36, 20);   // model, words 27-46
	FixAtaString(identify + 0x14, 10);   // serial, words 10-19
	FixAtaString(identify + 0x2E, 4);    // firmware, words 23-26
}

// 0x00090416
// Reads IDENTIFY data once (again if force).
static BYTE *ReadIdeIdentify(BOOL hdd, BOOL force)
{
	BYTE *identify = hdd ? s_hddIdentify : s_dvdIdentify;
	if (*(WORD *)identify != 0 && !force)
		return identify;
	IdentifyDevice(identify, hdd);
	return identify;
}

static void CopyIdeString(BOOL hdd, int offset, unsigned length, char *out, unsigned size)
{
	BYTE *identify = ReadIdeIdentify(hdd, FALSE);
	strncpy(out, (const char *)identify + offset, size < length ? size : length);
}

// 0x000905A2
void GetIdeModel(BOOL hdd, char *out, unsigned size)
{
	CopyIdeString(hdd, 0x36, 40, out, size);
}

// 0x00090567
void GetIdeSerial(BOOL hdd, char *out, unsigned size)
{
	CopyIdeString(hdd, 0x14, 20, out, size);
}

// 0x0009052C
void GetIdeFirmware(BOOL hdd, char *out, unsigned size)
{
	CopyIdeString(hdd, 0x2E, 8, out, size);
}

// 0x000904FC
// IDENTIFY word 128. "Enabled" also sets the supported bit; the locked bit
// (2) isn't passed on.
WORD GetHddSecurityStatus()
{
	BYTE security = ReadIdeIdentify(TRUE, FALSE)[0x100];
	WORD status = 0;
	if (security & 0x01)
		status = HDD_SECURITY_SUPPORTED;
	if (security & 0x02)
		status |= HDD_SECURITY_SUPPORTED | HDD_SECURITY_ENABLED;
	if (security & 0x08)
		status |= HDD_SECURITY_FROZEN;
	if (security & 0x10)
		status |= HDD_SECURITY_COUNT_EXPIRED;
	return status;
}

// 0x0009048B
// Quirks: both print IDENTIFY's bit masks, not mode numbers (word 64's
// supported PIO modes; word 88's high byte, the selected Ultra DMA mode as
// a bit: "Ultra DMA Mode 32" for mode 5).
void GetHddInfoText(int kind, char *out)
{
	const WORD *identify = (const WORD *)ReadIdeIdentify(TRUE, FALSE);
	if (kind == 1)
	{
		sprintf(out, "Cyl: %d  Heads: %d  Sectors/Track: %d", identify[1], identify[3], identify[6]);
	}
	else if (kind == 2)
	{
		WORD udma = identify[88];
		if ((udma >> 8) != 0)
			sprintf(out, "Ultra DMA Mode %d", udma >> 8);
		else
			sprintf(out, "PIO Mode %d", identify[64] & 0xFF);
	}
}

// --- Hard disk lock ----------------------------------------------------------

#define ATA_SECURITY_SET_PASSWORD 0xF1
#define ATA_SECURITY_DISABLE_PASSWORD 0xF6
// The master password UnleashX (like Team Assembly's tools) sets.
static const char s_szMasterPassword[] = "TEAMASSEMBLY";   // 0x0024C150
#define HDD_PASSWORD_LENGTH 20

// The length of an ATA string without its trailing spaces. Quirk: when
// everything after the first character is spaces, the full length.
static int TrimmedAtaLength(const char *s, int length)
{
	for (int i = length - 1; i > 0; i--)
	{
		if (s[i] != ' ')
			return i + 1;
	}
	return length;
}

// 0x00090C19
// The Xbox's user password for its disk: HMAC-SHA1 under the HDD key of
// the model and serial number. Quirk: the key's length is its strlen, so a
// key with a zero byte is cut short (the real one is 16 bytes).
static void ComputeHddPassword(BYTE *password)
{
	char key[17];
	memcpy(key, XboxHDKey, 16);
	key[16] = 0;
	char model[40] = {0};
	char serial[20] = {0};
	memcpy(model, HalDiskModelNumber.Buffer, sizeof(model));
	memcpy(serial, HalDiskSerialNumber.Buffer, sizeof(serial));
	int modelLength = TrimmedAtaLength(model, HalDiskModelNumber.Length < 40 ? HalDiskModelNumber.Length : 40);
	int serialLength = TrimmedAtaLength(serial, HalDiskSerialNumber.Length < 20 ? HalDiskSerialNumber.Length : 20);
	XKSHA1 sha1;
	sha1.HMAC_SHA1(password, (const BYTE *)key, (int)strlen(key), (const BYTE *)model, modelLength,
	               (const BYTE *)serial, serialLength);
}

static HANDLE OpenHddForWrite()
{
	ANSI_STRING name;
	RtlInitAnsiString(&name, "\\Device\\Harddisk0\\Partition0");
	OBJECT_ATTRIBUTES attributes = {NULL, &name, 0};
	IO_STATUS_BLOCK status;
	HANDLE disk;
	if (NtOpenFile(&disk, GENERIC_READ | GENERIC_WRITE | SYNCHRONIZE, &attributes, &status,
	               FILE_SHARE_READ | FILE_SHARE_WRITE, FILE_SYNCHRONOUS_IO_NONALERT) < 0)
		return NULL;
	return disk;
}

// A SECURITY command with its 512-byte password block: word 0 is 1 for the
// master password, the password follows. TRUE if sent and the drive didn't
// set the error bit.
static BOOL SendSecurityCommand(HANDLE disk, BYTE command, BOOL master, const void *password, size_t length)
{
	BYTE block[0x200];
	memset(block, 0, sizeof(block));
	block[0] = master ? 1 : 0;
	memcpy(block + 2, password, length);
	ATA_PASS_THROUGH ata;
	memset(&ata.IdeReg, 0, sizeof(ata.IdeReg));
	ata.IdeReg.bDriveHeadReg = 0xA0;
	ata.IdeReg.bCommandReg = command;
	ata.IdeReg.bHostSendsData = 1;
	ata.DataBufferSize = sizeof(block);
	ata.DataBuffer = block;
	DWORD returned;
	if (!DeviceIoControl(disk, IOCTL_IDE_PASS_THROUGH, &ata, sizeof(ata), &ata, sizeof(ata), &returned, NULL))
		return FALSE;
	return (ata.IdeReg.bCommandReg & 1) == 0;
}

static BOOL IsHddLocked(WORD status)
{
	return (status & HDD_SECURITY_SUPPORTED) && (status & HDD_SECURITY_ENABLED);
}

// 0x00090D42
// Sets the master password, then the user password; TRUE once the drive
// reports security enabled (checked up to 3 times), or right away when the
// drive has no security or is already locked. Quirk: a password whose
// first byte is 0 counts as none.
BOOL LockHdd()
{
	ReadIdeIdentify(TRUE, TRUE);
	WORD status = GetHddSecurityStatus();
	if (!(status & HDD_SECURITY_SUPPORTED) || (status & HDD_SECURITY_ENABLED))
		return TRUE;
	BYTE password[32] = {0};
	ComputeHddPassword(password);
	if (password[0] == 0)
		return FALSE;
	HANDLE disk = OpenHddForWrite();
	if (disk == NULL)
		return FALSE;

	BOOL locked = FALSE;
	if (SendSecurityCommand(disk, ATA_SECURITY_SET_PASSWORD, TRUE, s_szMasterPassword, sizeof(s_szMasterPassword)))
	{
		Sleep(50);
		if (SendSecurityCommand(disk, ATA_SECURITY_SET_PASSWORD, FALSE, password, HDD_PASSWORD_LENGTH))
		{
			for (int tries = 0; tries < 3; tries++)
			{
				ReadIdeIdentify(TRUE, TRUE);
				status = GetHddSecurityStatus();
				if (IsHddLocked(status))
					break;
			}
			locked = IsHddLocked(status);
		}
	}
	NtClose(disk);
	return locked;
}

// 0x00090EF8
// Removes the user password, or failing that the master password; TRUE
// once the drive reports security disabled.
BOOL UnlockHdd()
{
	ReadIdeIdentify(TRUE, TRUE);
	if (!IsHddLocked(GetHddSecurityStatus()))
		return FALSE;
	BYTE password[32] = {0};
	ComputeHddPassword(password);
	if (password[0] == 0)
		return FALSE;
	HANDLE disk = OpenHddForWrite();
	if (disk == NULL)
		return FALSE;

	BOOL done = SendSecurityCommand(disk, ATA_SECURITY_DISABLE_PASSWORD, FALSE, password, HDD_PASSWORD_LENGTH);
	if (!done)
	{
		Sleep(10);
		done = SendSecurityCommand(disk, ATA_SECURITY_DISABLE_PASSWORD, TRUE, s_szMasterPassword,
		                           sizeof(s_szMasterPassword));
	}
	NtClose(disk);
	if (!done)
		return FALSE;
	Sleep(10);
	ReadIdeIdentify(TRUE, TRUE);
	WORD status = GetHddSecurityStatus();
	return (status & HDD_SECURITY_SUPPORTED) && !(status & HDD_SECURITY_ENABLED);
}

// --- SMART ------------------------------------------------------------------

struct SSmartAttribute
{
	DWORD id;
	DWORD value;
	DWORD worst;
};

static HANDLE s_hSmartDisk = INVALID_HANDLE_VALUE;   // 0x0026BE78
static BOOL s_bSmartTemperature = TRUE;              // 0x0026D220 drive reports attribute 0xC2
static SSmartAttribute s_smartAttributes[11];        // 0x00272630

static void InitSmartCommand(ATA_PASS_THROUGH *ata, BYTE feature, BYTE *data)
{
	memset(ata, 0, sizeof(*ata));
	ata->IdeReg.bFeaturesReg = feature;
	ata->IdeReg.bSectorCountReg = 1;
	ata->IdeReg.bSectorNumberReg = 1;
	ata->IdeReg.bCylLowReg = SMART_CYL_LOW;
	ata->IdeReg.bCylHighReg = SMART_CYL_HIGH;
	ata->IdeReg.bDriveHeadReg = 0;
	ata->IdeReg.bCommandReg = ATA_SMART;
	ata->DataBufferSize = 0x200;
	ata->DataBuffer = data;
}

// 0x00090752
// Opens the disk for SMART and enables it. Returns TRUE if the drive accepted.
static BOOL OpenSmartDisk()
{
	OpenIdeDevice("\\Device\\Harddisk0\\Partition0", &s_hSmartDisk);
	if (s_hSmartDisk == INVALID_HANDLE_VALUE)
		return FALSE;

	BYTE data[0x200];
	ATA_PASS_THROUGH ata;
	InitSmartCommand(&ata, SMART_ENABLE_OPERATIONS, data);
	DWORD returned;
	if (!DeviceIoControl(s_hSmartDisk, IOCTL_IDE_PASS_THROUGH, &ata, sizeof(ata), &ata, sizeof(ata), &returned,
	                     NULL))
		return FALSE;
	// The status register comes back in the command register: not busy or in
	// error, ready, no fault or data request.
	BYTE status = ata.IdeReg.bCommandReg;
	return (status & 0x81) == 0 && (status & 0x40) != 0 && (status & 0x28) == 0;
}

// 0x00090453
void GetSmartAttribute(unsigned index, DWORD *id, DWORD *value, DWORD *worst)
{
	if (index >= 11)
		return;
	if (id != NULL)
		*id = s_smartAttributes[index].id;
	if (value != NULL)
		*value = s_smartAttributes[index].value;
	if (worst != NULL)
		*worst = s_smartAttributes[index].worst;
}

// 0x00090AB1
// Reads the SMART attributes (keeping the ones shown on the HDD info screen)
// and returns the normalized value of 0xC2, the temperature.
BYTE GetHddTemperature()
{
	if (!s_bSmartTemperature || (s_hSmartDisk == INVALID_HANDLE_VALUE && !OpenSmartDisk()))
		return 0;

	BYTE data[0x200];
	ATA_PASS_THROUGH ata;
	InitSmartCommand(&ata, SMART_READ_DATA, data);
	BYTE temperature = 0;
	DWORD returned;
	if (DeviceIoControl(s_hSmartDisk, IOCTL_IDE_PASS_THROUGH, &ata, sizeof(ata), &ata, sizeof(ata), &returned, NULL))
	{
		s_bSmartTemperature = FALSE;
		SSmartAttribute *out = s_smartAttributes;
		// 30 entries of 12 bytes after the 2-byte revision: id, flags (2),
		// value, worst, raw (6), reserved.
		for (int i = 0; i < 30; i++)
		{
			const BYTE *attr = data + 2 + i * 12;
			DWORD id = attr[0];
			switch (id)
			{
			case 0xC2:
				temperature = attr[3];
				s_bSmartTemperature = TRUE;
				// fall through
			case 0x01: case 0x03: case 0x04: case 0x05: case 0x07: case 0x09: case 0x0A: case 0x0C:
			case 0xC7: case 0xC8:
				out->id = id;
				out->value = attr[3];
				out->worst = attr[4];
				out++;
				break;
			}
		}
	}
	return temperature;
}

// --- BIOS flash -------------------------------------------------------------

// The flash is mapped at the top 16 MB of the address space.
#define FLASH_BYTE(offset) (*(volatile BYTE *)(0xFF000000 + (offset)))

class CFlash
{
public:
	WORD ReadId();     // 0x00031A83
	BYTE Reset();      // 0x00031A66
};

// 0x00031A66
BYTE CFlash::Reset()
{
	FLASH_BYTE(0x555) = 0xAA;
	FLASH_BYTE(0x2AA) = 0x55;
	FLASH_BYTE(0x555) = 0xF0;
	return FLASH_BYTE(0);
}

// 0x00031A83
// JEDEC autoselect: manufacturer in the high byte, device in the low byte.
WORD CFlash::ReadId()
{
	FLASH_BYTE(0x555) = 0xAA;
	FLASH_BYTE(0x2AA) = 0x55;
	FLASH_BYTE(0x555) = 0x90;
	BYTE manufacturer = FLASH_BYTE(0);
	BYTE device = FLASH_BYTE(1);
	Reset();
	return (WORD)((manufacturer << 8) | device);
}

static CFlash *s_pFlash;   // 0x00277654

// 0x000A2202
static WORD ReadFlashChipId()
{
	if (s_pFlash == NULL)
		s_pFlash = new CFlash;
	WORD id = 0;
	if (s_pFlash != NULL)
	{
		id = s_pFlash->ReadId();
		delete s_pFlash;
		s_pFlash = NULL;
	}
	return id;
}

struct SFlashChip
{
	const char *name;
	WORD manufacturer;
	WORD device;
	WORD unknown;
	WORD size64K;   // size in 64 KB blocks
};

// 0x0026B9D8
static const SFlashChip kFlashChips[] = {
	{"A29002 - AMIC", 0x37, 0x8C, 0, 4},
	{"Am29F016 - AMD", 0x01, 0xAD, 0, 16},
	{"Am29F080B - AMD", 0x01, 0xD5, 0, 16},
	{"Am29LV800B - AMD", 0x01, 0x5B, 0, 16},
	{"Am29LV800B - AMD", 0x01, 0xDA, 0, 16},
	{"MBM29F080A - Fujitsu", 0x04, 0xD5, 0, 16},
	{"MBM29F040C - Fujitsu", 0x04, 0xA4, 0, 8},
	{"MBM29LV400BC - Fujitsu", 0x04, 0xBA, 0, 8},
	{"HY29F002 - Hynix", 0xAD, 0xB0, 0, 4},
	{"HY29F080 - Hynix", 0xAD, 0xD5, 0, 16},
	{"49LF020 - SST", 0xBF, 0x61, 0, 4},
	{"49LF020A - SST", 0xBF, 0x52, 0, 4},
	{"M29F080A - ST", 0x20, 0xF1, 0, 16},
	{"M29F002BT70N1 - ST", 0x20, 0xB0, 0, 4},
	{"W29f020 - Winbond", 0xDA, 0x8C, 0, 4},
	{"A29002 - AMIC", 0x37, 0x8C, 0, 4},
	{"SHARP - LH28F008SCT", 0x89, 0xA6, 0, 16},
	{"SST49LF040 - SST", 0xBF, 0x53, 0, 8},
	{"Protected", 0x09, 0x00, 0, 4},
	{"Unknown", 0x00, 0x00, 0, 4},
};

// 0x000A3EA0
std::string GetFlashChipName(BOOL withId)
{
	std::string name;
	name = "Unknown";
	WORD id = ReadFlashChipId();
	if (id != 0 && id != 0xFFFF)
	{
		if (withId)
			StrFormat(name, "%04X", id);
		for (int i = 0; kFlashChips[i].manufacturer != 0; i++)
		{
			if (((kFlashChips[i].manufacturer << 8) | kFlashChips[i].device) == id)
			{
				name = kFlashChips[i].name;
				break;
			}
		}
	}
	return name;
}
