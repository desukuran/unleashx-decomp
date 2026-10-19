#include "FileUtil.h"

#include <cctype>
#include <cstdio>
#include <cstring>

#include "../settings/AppSettings.h"
#include "StrUtil.h"

#ifdef _XBOX
#include "XboxKernel.h"
#endif

// 0x0009DE34
BOOL FileExists(const char *path)
{
	if (path == NULL || strlen(path) == 0)
		return FALSE;
	DWORD attributes = GetFileAttributesA(path);
	return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

// 0x0009DDFC
BOOL DirectoryExists(const char *path)
{
	if (path == NULL || strlen(path) == 0)
		return FALSE;
	DWORD attributes = GetFileAttributesA(path);
	return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

// 0x000A4AAD
BOOL IsDriveAvailable(char drive)
{
	std::string root;
	StrFormat(root, "%c:\\", toupper(drive));
	return GetFileAttributesA(root.c_str()) != INVALID_FILE_ATTRIBUTES;
}

// 0x000A3745
std::string ReadTextFile(const char *path)
{
	std::string text;

	FILE *fp = fopen(path, "rb");
	if (fp == NULL)
		return std::string("");

	fseek(fp, 0, SEEK_END);
	long size = ftell(fp);
	if (size > 0)
	{
		fseek(fp, 0, SEEK_SET);
		char *buf = new char[size];
		if (buf == NULL)
		{
			fclose(fp);
			return std::string("");
		}
		memset(buf, 0, size);
		if (fread(buf, size, 1, fp) == 0)
		{
			delete[] buf;
			fclose(fp);
			return std::string("");
		}
		text.assign(buf, size);
		delete[] buf;
	}
	fclose(fp);
	return text;
}

// 0x000A3D25
// Returns the NT device path a drive letter's symbolic link points to,
// e.g. 'E' -> "\Device\Harddisk0\Partition1".
HRESULT GetDriveDevicePath(char drive, std::string &devicePath)
{
#ifdef _XBOX
	HRESULT hr = E_FAIL;
	if (isalpha(drive))
	{
		char linkName[] = "\\??\\D:";
		linkName[4] = (char)toupper(drive);

		ANSI_STRING name = { 6, 7, linkName };
		OBJECT_ATTRIBUTES attributes = { NULL, &name, OBJ_CASE_INSENSITIVE };
		HANDLE link = NULL;
		NtOpenSymbolicLinkObject(&link, &attributes);
		if (link != NULL)
		{
			// The original never frees this buffer.
			char *buffer = (char *)calloc(0x208, 1);  // TODO: verify allocator (0x0009D427)
			ANSI_STRING target = { 0, 0x208, buffer };
			ULONG length = 0;
			NtQuerySymbolicLinkObject(link, &target, &length);
			const char *result = "";
			if (target.Buffer != NULL && length != 0)
			{
				result = target.Buffer;
				hr = S_OK;
			}
			devicePath = result;
			NtClose(link);
			return hr;
		}
	}
	devicePath = "";
	return hr;
#else
	(void)drive;
	devicePath = "";
	return E_FAIL;
#endif
}

// 0x000A3934
// The kernel's name for the running XBE, with the extension normalized to
// ".xbe" and anything after it cut off.
std::string GetXbePath()
{
	std::string path;
#ifdef _XBOX
	path = XeImageFileName->Buffer;
#endif
	StrReplaceAll(path, ".Xbe", ".xbe");
	StrReplaceAll(path, ".XBE", ".xbe");
	int ext = StrFind(path, ".xbe");
	if (ext != -1)
		path = StrLeft(path, ext + 4);
	return path;
}

// 0x000AC73F
// - "X:\..." stays as it is.
// - "\..." gets the home drive in front.
// - ".\..." and other relative paths are relative to the home directory;
//   each "..\" goes up one level (never above the drive root).
// - Unless running from DVD, "D:\" means the home directory too.
// The result is lowercased (the input is copied with _strdup, never freed)
// and has all '"' removed.
void ResolvePath(const char *path, std::string &out)
{
	const std::string &home = CAppSettings::s_pInstance->m_strHomePath;

	out = StrToLower(path);
	if (out.size() == 0)
		out = home;
	StrReplaceAll(out, "/", "\\");

	if (!CAppSettings::s_pInstance->m_bRunFromDVD)
	{
		StrReplaceAll(out, "D:\\", home.c_str());
		StrReplaceAll(out, "d:\\", home.c_str());
	}

	if (StrFind(out, ":\\") == -1)
	{
		std::string base;
		base = "";

		if (StrLeft(out, 1).compare("\\") == 0)
		{
			base = StrLeft(home, 2);
			base += out;
			out = base.c_str();
			base.clear();
		}
		else if (StrLeft(out, 2).compare(".\\") == 0)
		{
			base = home;
			StrReplaceAll(out, ".\\", "");
			base += out;
			out = base.c_str();
			base.clear();
		}
		else
		{
			base = home.c_str();
			if (StrFind(out, "..\\") != -1)
			{
				// Each "..\" becomes ".." (one char shorter), so the size
				// difference is the number of levels to go up.
				int before = (int)out.size();
				StrReplaceAll(out, "..\\", "..");
				int levels = before - (int)out.size();

				if (StrRight(base, 1).compare("\\") == 0)
					base = StrLeft(base, (int)base.size() - 1);
				int slash = StrReverseFind(base, "\\");
				while (levels > 0 && slash > 2)
				{
					base = StrLeft(base, slash);
					slash = StrReverseFind(base, "\\", slash - 1);
					levels--;
				}
				StrReplaceAll(out, "..", "");
			}
			StrAddBackslash(base);
			base += out;
			out = base.c_str();
			base.clear();
			StrReplaceAll(out, "\\\\", "\\");
		}
	}

	StrRemoveChar(out, '"');
}
