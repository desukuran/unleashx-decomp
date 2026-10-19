// CFTPServerList: the FTP manager's site list. See FtpManager.h.
//
// FtpSrv.xml (in the data folder) looks like
//   <FTPSite>
//     <Site Host="192.168.0.2" Port="21" User="xbox" Pass="eGJveA==" PASV="Yes"
//           RemotePath="/E/Games" LocalPath="E:\Games">Living room</Site>
//   </FTPSite>
// The password is Base64.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>

#include "../common/Base64.h"
#include "../common/FileUtil.h"
#include "../common/StrUtil.h"
#include "../input/Gamepad.h"
#include "../net/Network.h"
#include "../settings/AppSettings.h"
#include "../xml/XmlUtil.h"
#include "FtpManager.h"

// 0x0001E5A9 (this in esi)
SFTPSiteInfo::SFTPSiteInfo()
{
	name[0] = 0;
	host[0] = 0;
	user[0] = 0;
	password[0] = 0;
	unknown100 = 0;
	saved = FALSE;
	port = 21;
	passive = TRUE;
}

// 0x0001E60C (this in esi, port in eax)
SFTPSiteInfo::SFTPSiteInfo(const wchar_t *siteName, const char *siteHost, int sitePort, const char *siteUser,
                           const char *sitePassword, BOOL sitePassive, DWORD siteUnknown100, BOOL siteSaved,
                           const char *siteRemotePath, const char *siteLocalPath)
{
	port = sitePort;
	passive = sitePassive;
	unknown100 = siteUnknown100;
	saved = siteSaved;
	memset(name, 0, sizeof(name));
	memset(host, 0, sizeof(host));
	memset(user, 0, sizeof(user));
	memset(password, 0, sizeof(password));
	// (strncpy: a name or host that fills its field isn't terminated.)
	wcsncpy(name, siteName, 50);
	strncpy(host, siteHost, 100);
	if (siteUser == NULL)
		user[0] = 0;
	else
		strncpy(user, siteUser, 20);
	if (sitePassword == NULL)
		password[0] = 0;
	else
		strncpy(password, sitePassword, 20);
	if (siteRemotePath != NULL)
		remotePath = siteRemotePath;
	if (siteLocalPath != NULL)
		localPath = siteLocalPath;
}

// 0x00122F45 (this in eax)
CFTPServerList::CFTPServerList()
{
	m_fY = 50.0f;
	m_fX = 50.0f;
	m_fWidth = 540.0f;
	m_fHeight = 380.0f;
	m_nMaxNameLength = 0;
	m_pFont = NULL;
	m_nSelected = 0;
	m_nTop = 0;
	m_nBottom = 0;
	m_nCount = 0;
	m_bActive = FALSE;
	m_unknown500 = 0;
	m_nFilter = FILTER_FOLDERS;
	memset(m_szTitle, 0, sizeof(m_szTitle));
	m_unknown4FC = 0;
}

// 0x00120BD3
CFTPServerList::~CFTPServerList()
{
	m_pFont = NULL;
	m_sites.clear();
	m_xml.Clear();
}

// 0x0011F1D6
unsigned __stdcall CFTPServerList::LoadThread(void *param)
{
	((IFileFolder *)param)->LoadList();
	return 0;
}

// 0x0011F638
void CFTPServerList::Refresh()
{
	m_bListed = FALSE;
	HANDLE thread = (HANDLE)_beginthreadex(NULL, 0, LoadThread, this, CREATE_SUSPENDED, NULL);
	if (thread == NULL)
	{
		LoadList();
	}
	else
	{
		ResumeThread(thread);
		CloseHandle(thread);
	}
	m_dwRefreshTick = GetTickCount();
}

// 0x0012448B
void CFTPServerList::LoadList()
{
	m_bListed = FALSE;
	while (!m_bNotDrawing)
		Sleep(1);
	Sleep(5);
	m_pCurrent = NULL;
	m_nFolderCount = 0;
	m_nFileCount = 0;
	// (Only the folder items are freed: sites are folders.)
	for (std::vector<SFolderItem *>::iterator it = m_folders.begin(); it != m_folders.end(); ++it)
		delete *it;
	m_folders.clear();
	m_ullTotalSize = 0;
	memset(m_szTotalSize, 0, sizeof(m_szTotalSize));
	ListSites();
	m_ullTotalSize = 0;
	m_items.clear();
	if (m_nFilter != FILTER_FILES && m_folders.size() != 0)
	{
		for (std::vector<SFolderItem *>::iterator it = m_folders.begin(); it != m_folders.end(); ++it)
		{
			if (*it != NULL)
				m_items.push_back(*it);
		}
	}
	if (m_nFilter != FILTER_FOLDERS && m_files.size() != 0)
	{
		for (std::vector<SFolderItem *>::iterator it = m_files.begin(); it != m_files.end(); ++it)
		{
			if (*it != NULL)
				m_items.push_back(*it);
		}
	}
	m_szTotalSize[0] = 0;
	FitList();
	m_bListed = TRUE;
}

// 0x00122F2B
HRESULT CFTPServerList::ProcessInput(int *action, SGamepadState **pad)
{
	if (*action == INPUT_A)
		return S_OK;
	return IFileFolder::ProcessInput(action, pad);
}

// 0x00123F5B
void CFTPServerList::ListSites()
{
	WIN32_FIND_DATAA data;
	memset(&data, 0, sizeof(data));
	data.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
	m_sites.clear();
	if (g_pFtpSettings->m_ftp.m_bClientDetectPeers)
		ScanNetworkClients();

	// FtpSrv.xml: created empty when missing, read once.
	std::string path = g_pFtpSettings->m_strDataPath;
	path.append("FtpSrv.xml");
	m_xml.m_pCurrent = &m_xml.m_root;
	if (m_xml.m_root.m_strName.size() == 0)
	{
		m_xml.m_pCurrent = &m_xml.m_root;
		m_xml.m_root.m_strName = "FTPSite";
	}
	if (!FileExists(path.c_str()))
	{
		m_xml.SaveAs(path.c_str());
	}
	else
	{
		std::string loaded = m_xml.m_strFileName;
		if (loaded.size() == 0)
			m_xml.Load(path.c_str());
	}

	// The saved sites (a name, a host and a port each).
	m_xml.m_pCurrent = &m_xml.m_root;
	for (CXmlElement *site = m_xml.m_root.FindFirstChild("Site"); site != NULL;)
	{
		std::string host, user, password, remotePath, localPath;
		long port = 0;
		BOOL passive = TRUE;
		if (site->m_strValue.size() != 0)
		{
			wchar_t name[0x34];
			StrUtf8ToWide(site->m_strValue.c_str(), name, 0x32);
			for (std::vector<CXmlAttribute *>::iterator it = site->m_attributes.begin(); it != site->m_attributes.end();
			     ++it)
			{
				CXmlAttribute *attribute = *it;
				if (attribute == NULL)
					continue;
				if (StrCmpNoCase(attribute->m_strName, "Host") == 0)
				{
					host = attribute->m_strValue;
				}
				else if (StrCmpNoCase(attribute->m_strName, "Port") == 0)
				{
					port = atol(attribute->m_strValue.c_str());
				}
				else if (StrCmpNoCase(attribute->m_strName, "User") == 0)
				{
					user = attribute->m_strValue;
				}
				else if (StrCmpNoCase(attribute->m_strName, "Pass") == 0)
				{
					password = attribute->m_strValue;
					if (password.size() != 0)
					{
						std::string decoded;
						if (Base64Decode(password.c_str(), decoded))
							password = decoded;   // (via a _strdup copy that leaks)
						else
							password.clear();
					}
				}
				else if (StrCmpNoCase(attribute->m_strName, "PASV") == 0)
				{
					passive = !IsFalseString(attribute->m_strValue.c_str());
				}
				else if (StrCmpNoCase(attribute->m_strName, "RemotePath") == 0)
				{
					remotePath = attribute->m_strValue;
				}
				else if (StrCmpNoCase(attribute->m_strName, "LocalPath") == 0)
				{
					localPath = attribute->m_strValue;
				}
			}
			if (host.size() != 0 && port != 0)
			{
				m_sites.push_back(SFTPSiteInfo(name, host.c_str(), (int)port, user.c_str(), password.c_str(), passive, 0,
				                               TRUE, remotePath.c_str(), localPath.c_str()));
				memset(data.cFileName, 0, sizeof(data.cFileName));
				strcpy(data.cFileName, site->m_strValue.c_str());
				AddEntry(&data, (ULONGLONG)port, 0, FALSE);
			}
		}
		m_xml.m_pCurrent = &m_xml.m_root;
		site = m_xml.m_root.FindNextChild();
	}

	// Then the Xboxes that answered the scan ("[ip]" without a name).
	for (std::vector<SNetClient>::iterator it = g_netClients.begin(); it != g_netClients.end(); ++it)
	{
		wchar_t name[0x34];
		if (strlen(it->name) == 0)
			swprintf(name, 0x34, L"[%hs]", it->ip);
		else
			swprintf(name, 0x34, L"%hs", it->name);
		m_sites.push_back(SFTPSiteInfo(name, it->ip, (int)it->ftpPort, it->ftpUser, it->ftpPassword, TRUE,
		                               (DWORD)it->unknown54, FALSE, NULL, NULL));
		memset(data.cFileName, 0, sizeof(data.cFileName));
		sprintf(data.cFileName, "%S", name);
		AddEntry(&data, (ULONGLONG)it->ftpPort, 0, TRUE);
	}
	m_nCount = (int)m_sites.size();
}

// 0x0011FB95 (this in ecx). (m_pCurrent must be set.)
SFTPSiteInfo CFTPServerList::GetSelectedSite()
{
	for (std::vector<SFTPSiteInfo>::iterator it = m_sites.begin(); it != m_sites.end(); ++it)
	{
		if (wcscmp(it->name, m_pCurrent->m_pszName) == 0)
			return *it;
	}
	return SFTPSiteInfo();
}

// 0x00124695 (element in eax, this in edi)
void CFTPServerList::WriteSite(CXmlElement *element, const char *host, int port, const char *user, const char *password,
                               BOOL passive, const char *remotePath, const char *localPath)
{
	if (element == NULL)
		return;
	std::string text;
	element->SetAttribute("Host", host);
	StrFormat(text, "%d", port);
	element->SetAttribute("Port", text.c_str());
	element->SetAttribute("User", user);
	element->SetAttribute("Pass", password);
	element->SetAttribute("PASV", passive ? "Yes" : "No");
	element->SetAttribute("RemotePath", remotePath);
	element->SetAttribute("LocalPath", localPath);
	m_xml.Save(TRUE);
	m_xml.Refresh();
	Refresh();
}

// 0x00124779 (this in ebx)
void CFTPServerList::AddSite(const char *name, const char *host, int port, const char *user, const char *password,
                             BOOL passive, const char *remotePath, const char *localPath)
{
	if (name == NULL || strlen(name) == 0)
		return;
	// A free name: "name", else "name(1)", "name(2)" ...
	std::string siteName = name;
	m_xml.m_pCurrent = &m_xml.m_root;
	CXmlElement *site = m_xml.m_root.FindFirstChild("Site");
	int n = 1;
	while (site != NULL)
	{
		if (site->m_strValue.compare(siteName) == 0)
		{
			StrFormat(siteName, "%s(%d)", name, n);
			m_xml.m_pCurrent = &m_xml.m_root;
			site = m_xml.m_root.FindFirstChild("Site");
			n++;
		}
		else
		{
			m_xml.m_pCurrent = &m_xml.m_root;
			site = m_xml.m_root.FindNextChild();
		}
	}
	std::string encoded;
	if (password != NULL && strlen(password) != 0)
	{
		encoded = password;
		if (encoded.size() != 0)
		{
			std::string base64;
			if (Base64Encode(encoded.c_str(), base64))
				encoded = base64;
		}
	}
	m_xml.m_pCurrent = &m_xml.m_root;
	CXmlElement *element = AddChildElement(&m_xml.m_root, "Site", siteName.c_str(), NULL);
	WriteSite(element, host, port, user, encoded.c_str(), passive, remotePath, localPath);
}

// 0x00124928 (this in eax)
void CFTPServerList::SaveSite(const char *name, const char *newName, const char *host, int port, const char *user,
                              const char *password, BOOL passive, const char *remotePath, const char *localPath)
{
	std::string encoded = password;
	if (encoded.size() != 0)
	{
		std::string base64;
		if (Base64Encode(encoded.c_str(), base64))
			encoded = base64;
	}
	m_xml.m_pCurrent = &m_xml.m_root;
	for (CXmlElement *site = m_xml.m_root.FindFirstChild("Site"); site != NULL;)
	{
		if (site->m_strValue.compare(name) == 0)
		{
			WriteSite(site, host, port, user, encoded.c_str(), passive, remotePath, localPath);
			return;
		}
		m_xml.m_pCurrent = &m_xml.m_root;
		site = m_xml.m_root.FindNextChild();
	}
	AddSite(newName, host, port, user, password, passive, remotePath, localPath);
}

// 0x00123ECD
void CFTPServerList::DeleteSite(const char *name)
{
	if (name == NULL || strlen(name) == 0)
		return;
	m_xml.m_pCurrent = &m_xml.m_root;
	for (CXmlElement *site = m_xml.m_root.FindFirstChild("Site"); site != NULL;)
	{
		if (site->m_strValue.compare(name) == 0)
		{
			if (!m_xml.RemoveElement(site))
				return;
			if (!m_xml.Save(FALSE))
				return;
			Refresh();
			return;
		}
		m_xml.m_pCurrent = &m_xml.m_root;
		site = m_xml.m_root.FindNextChild();
	}
}
