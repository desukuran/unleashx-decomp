// UnleashX build shim: XBMC's CUtil::CreateDirectoryEx, the one call
// UnrarXLib makes. (Not XBMC code.)
#pragma once
#include <xtl.h>
#include <string>

class CUtil
{
public:
	// Creates the folder and the ones above it.
	static bool CreateDirectoryEx(const std::string &path)
	{
		std::string part;
		for (size_t i = 0; i < path.size(); i++)
		{
			part += path[i];
			if ((path[i] == '\\' || path[i] == '/') && i > 2)
				CreateDirectoryA(part.c_str(), NULL);
		}
		CreateDirectoryA(path.c_str(), NULL);
		return GetFileAttributesA(path.c_str()) != (DWORD)-1;
	}
};
