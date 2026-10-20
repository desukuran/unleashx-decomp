// UnleashX build shim: XBMC's URIUtils::GetDirectory. (Not XBMC code.)
#pragma once
#include <string>

class URIUtils
{
public:
	// The path up to and including its last separator.
	static std::string GetDirectory(const std::string &path)
	{
		size_t slash = path.find_last_of("\\/");
		return slash == std::string::npos ? std::string() : path.substr(0, slash + 1);
	}
};
