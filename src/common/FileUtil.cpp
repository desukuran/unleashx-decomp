#include "FileUtil.h"

#include <cctype>
#include <cstdio>
#include <cstring>

#include "../settings/AppSettings.h"
#include "StrUtil.h"

#include "XboxKernel.h"

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
}

// 0x000A3934
// The kernel's name for the running XBE, with the extension normalized to
// ".xbe" and anything after it cut off.
std::string GetXbePath()
{
	std::string path;
	if (XeImageFileName != NULL)
		path = XeImageFileName->Buffer;
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

// The characters FATX allows in names (plus '\\' and ':' for whole paths).
// 0x0025AF78, through the pointer at 0x0026BC5C
static const char *const s_fatxCharacters =
	" !#$%&'()-.@[]^_`{}~0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ\\:";

// 0x0009F0CA
BOOL IsFatxName(const char *name)
{
	if (name == NULL || strlen(name) == 0 || strlen(name) > 42)
		return FALSE;
	for (; *name != '\0'; name++)
	{
		if (*name < 0)
			continue;   // characters >= 0x80 aren't checked
		if (strchr(s_fatxCharacters, *name) == NULL)
			return FALSE;
	}
	return TRUE;
}

// 0x000A4630
// The name is shortened by cutting it at the last space, then the last '.',
// '-', ':' or ',' (the extension is kept aside), and finally hard at the
// limit. Quirks: the character filter runs over the whole path, so folder
// names lose their illegal characters too (and characters >= 0x80, which
// IsFatxName() accepts, are dropped); ".." and "\\\\" are collapsed
// afterwards.
BOOL MakeFatxPath(const char *path, std::string &out)
{
	std::string clean;
	std::string dir;
	std::string name = path;
	int slash = StrReverseFind(name, "\\", -1);
	if (slash != -1)
	{
		dir = StrLeft(name, slash + 1);
		name = StrMid(name, slash + 1, name.size() - (slash + 1));
	}
	if (IsFatxName(name.c_str()))
		return FALSE;

	for (const char *p = path; *p != '\0'; p++)
	{
		// (characters >= 0x80 never match)
		if (strchr(s_fatxCharacters, *p) != NULL)
			clean.append(1, *p);
	}
	slash = StrReverseFind(clean, "\\", -1);
	if (slash != -1)
	{
		dir = StrLeft(clean, slash + 1);
		clean = StrMid(clean, slash + 1, clean.size() - (slash + 1));
	}

	if ((int)clean.size() <= 42)
	{
		std::string joined;
		StrFormat(joined, "%s%s", dir.c_str(), clean.c_str());
		clean = joined;
	}
	else
	{
		std::string extension;
		int limit = 42;
		int dot = StrReverseFind(clean, ".", -1);
		if (dot != -1)
		{
			extension = StrMid(clean, dot, clean.size() - dot);
			clean = StrLeft(clean, dot);
			limit = 42 - (int)extension.size();
		}

		int pos;
		while ((pos = StrReverseFind(clean, " ", -1)) != -1 && (int)clean.size() > limit)
			clean = StrLeft(clean, pos);
		if ((int)clean.size() > 38)
		{
			while ((pos = StrReverseFind(clean, ".", -1)) != -1 && (int)clean.size() > limit)
				clean = StrLeft(clean, pos);
			if ((int)clean.size() > 38)
			{
				const char *separators[3] = { "-", ":", "," };
				BOOL done = FALSE;
				for (int i = 0; i < 3 && !done; i++)
				{
					while ((pos = StrReverseFind(clean, separators[i], -1)) != -1)
					{
						if ((int)clean.size() <= limit)
						{
							done = TRUE;
							break;
						}
						clean = StrLeft(clean, pos);
					}
					if ((int)clean.size() <= limit)
						done = TRUE;
				}
				if (!done)
				{
					if (extension.size() != 0)
						clean = StrLeft(clean, limit);
					else if ((int)clean.size() > 42)
						clean = StrLeft(clean, 42);
				}
			}
		}
		std::string joined;
		StrFormat(joined, "%s%s%s", dir.c_str(), clean.c_str(), extension.c_str());
		clean = joined;
	}

	StrReplaceAll(clean, "..", ".");
	StrReplaceAll(clean, "\\\\", "\\");
	out = clean;
	return TRUE;
}


// 0x000A439A
BOOL WriteBinaryFile(const char *path, const void *data, DWORD size)
{
	BOOL ok = FALSE;
	if (size == 0 || data == NULL)
		return FALSE;
	std::string dir = path;
	int slash = StrReverseFind(dir, "\\", -1);
	if (slash != -1)
	{
		dir = StrLeft(dir, slash);
		CreateDirectoryTree(dir.c_str());
	}
	SetFileAttributesA(path, FILE_ATTRIBUTE_NORMAL);
	HANDLE file = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (file != INVALID_HANDLE_VALUE)
	{
		DWORD written = 0;
		if (WriteFile(file, data, size, &written, NULL) == TRUE)
			ok = TRUE;
		CloseHandle(file);
	}
	return ok;
}

// 0x000A3F51
BOOL CreateDirectoryTree(const char *path)
{
	if (path == NULL || strlen(path) == 0)
		return FALSE;
	std::string all = path;
	StrReplaceAll(all, "/", "\\");
	if (DirectoryExists(path))
		return TRUE;
	// Each folder on the way, from the drive root.
	std::string part = StrLeft(all, 3);
	int pos = 3;
	while (all.compare(part) != 0)
	{
		if (!DirectoryExists(part.c_str()) && !CreateDirectoryA(part.c_str(), NULL))
			return FALSE;
		pos = StrFind(all, "\\", pos + 1);
		if (pos == -1)
			break;
		part = StrLeft(all, pos);
	}
	// (path as given, slashes and all)
	if (DirectoryExists(path))
		return TRUE;
	return CreateDirectoryA(path, NULL) != 0;
}

// 0x000A4B0E
std::string GetPathDirectory(const char *path)
{
	std::string dir;
	std::string all = path;
	int dot = StrReverseFind(all, ".", -1);
	int slash = StrReverseFind(all, "\\", -1);
	if (dot >= 0 && slash >= 0 && dot <= slash && slash != (int)all.size() - 1)
		dir = all;                  // "C:\a.b\c": a folder
	else if (slash < 0 || dot < 0)
	{
		if (dot != -1)
			dir = "";               // "name.ext"
		else if (slash == -1)
			dir = all;              // "name"
		else
			dir = StrLeft(all, slash);   // "C:\Games\Halo": the last part counts as a file
	}
	else
		dir = StrLeft(all, slash);
	// No trailing backslash, except on a drive root.
	slash = StrReverseFind(dir, "\\", -1);
	if (slash > 0 && slash == (int)dir.size() - 1)
		dir = StrLeft(dir, slash);
	if (StrReverseFind(dir, ":", -1) == 1 && dir.size() == 2)
		dir.append("\\");
	return dir;
}
