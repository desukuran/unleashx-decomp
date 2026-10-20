// The ISO 9660 reader (xbox/Iso9660.cpp) on an image made by xorriso, with
// a CIoSupport that reads sectors from the image file.
//   iso9660_test <image.iso>
// The image holds README.TXT ("Hello, ISO!\n") and Folder/Big.bin (5000
// bytes of i & 0xFF), with Joliet names.
#include <cstdio>
#include <cstring>

#include "../src/thirdparty/Iso9660.h"
#include "../src/thirdparty/IoSupport.h"

static FILE *s_image;
HANDLE g_hIso9660Device;
CIoSupport g_ioSupport;

CIoSupport::CIoSupport() {}
CIoSupport::~CIoSupport() {}
HANDLE CIoSupport::OpenCDROM(DWORD) { return (HANDLE)s_image; }
void CIoSupport::CloseCDROM(HANDLE) {}
int CIoSupport::ReadSector(HANDLE, DWORD sector, char *buffer)
{
	if (fseek(s_image, (long)sector * 2048, SEEK_SET) != 0 || fread(buffer, 1, 2048, s_image) != 2048)
		return -1;
	return 2048;
}

static int failures = 0;

static void expect(bool ok, const char *what)
{
	if (!ok)
	{
		printf("FAIL: %s\n", what);
		failures++;
	}
}

int main(int argc, char **argv)
{
	if (argc < 2 || (s_image = fopen(argv[1], "rb")) == NULL)
	{
		printf("usage: iso9660_test <image.iso>\n");
		return 2;
	}
	iso9660 *iso = new iso9660;
	expect(strcmp(iso->GetVolumeName(), "TESTDISC") == 0, "volume name");
	expect(iso->GetDiscSize() > 0, "disc size");

	WIN32_FIND_DATAA data;
	int files = 0, folders = 0;
	HANDLE find = iso->FindFirstFile("D:\\*", &data);
	expect(find != INVALID_HANDLE_VALUE, "list the root");
	if (find != INVALID_HANDLE_VALUE)
	{
		do
		{
			if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
				folders += strcmp(data.cFileName, "Folder") == 0;
			else
				files += strcmp(data.cFileName, "README.TXT") == 0 && data.nFileSizeLow == 12;
		} while (iso->FindNextFile(find, &data));
		iso->FindClose(find);
	}
	expect(files == 1 && folders == 1, "root entries with Joliet names and sizes");

	find = iso->FindFirstFile("D:\\folder\\big.BIN", &data);
	expect(find != INVALID_HANDLE_VALUE && data.nFileSizeLow == 5000, "a file by name, in any case");
	iso->FindClose(find);
	expect(iso->FindFirstFile("D:\\Folder\\nothing", &data) == INVALID_HANDLE_VALUE, "a missing name");

	HANDLE file = iso->OpenFile("D:\\README.TXT");
	char text[32] = {0};
	expect(file != INVALID_HANDLE_VALUE && iso->ReadFile(file, (BYTE *)text, sizeof(text)) == 12 &&
	           strcmp(text, "Hello, ISO!\n") == 0, "read a small file");
	expect(iso->ReadFile(file, (BYTE *)text, sizeof(text)) == 0, "end of file");
	iso->CloseFile(file);

	file = iso->OpenFile("\\Folder\\Big.bin");
	expect(iso->GetFileSize(file) == 5000, "file size");
	BYTE big[6000];
	BOOL ok = iso->Seek(file, 2040, SEEK_SET) == 2040 && iso->ReadFile(file, big, 3000) == 2960;
	for (int i = 0; ok && i < 2960; i++)
		ok = big[i] == (BYTE)((2040 + i) & 0xFF);
	expect(ok != FALSE, "seek and read across sectors");
	iso->CloseFile(file);

	delete iso;
	fclose(s_image);
	if (failures == 0)
		printf("ok\n");
	return failures != 0;
}
