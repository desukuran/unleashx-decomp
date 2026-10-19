// Network startup, status and the "ping" scan for other Xboxes. The XNet
// stack is started on a thread so the dashboard comes up without waiting for
// DHCP. If the cable is unplugged, a monitor thread stops the stack, and starts
// it again when the link comes back.
#pragma once

#include <string>
#include <vector>

#include "../common/Platform.h"
#include "XNet.h"

// Another Xbox that answered the scan. The reply is
// "<name>;<ftp user>;<ftp password>;<ftp port>;<n>\r" and each one becomes an
// FTP site in the file manager. 0x58 bytes.
struct SNetClient
{
	char name[32];           // +0x00 "[<ip>]" if the reply had no name
	char ip[16];             // +0x20
	char ftpUser[16];        // +0x30
	char ftpPassword[16];    // +0x40
	long ftpPort;            // +0x50
	long unknown54;          // +0x54 last reply field, passed on with the FTP site TODO: what is it
};

extern BYTE g_bNetworkStarted;             // 0x00276260
extern BOOL g_bNetworkStopping;            // 0x00276264
extern HANDLE g_hNetworkEvent;             // 0x00276268
extern DWORD g_dwIPAddress;                // 0x002713A4 the title address once known, else 0
extern XNetConfigParams g_netConfig;       // 0x002713A8
extern XNADDR g_xnAddr;                    // 0x00271A94
extern DWORD g_dwXnAddrStatus;             // 0x0026BBD8 XNET_GET_XNADDR_*
extern std::vector<SNetClient> g_netClients;   // 0x00278670

// waitSeconds > 0: if the link is up, wait (on a thread) up to that many
// seconds before starting.
void StartNetwork(int waitSeconds);        // 0x000AF81C
HRESULT StopNetwork();                     // 0x000AEB5B
BOOL HasNoNetworkAddress();                // 0x000AEBF9
BOOL IsDhcpAddress();                      // 0x000AEC23
BOOL IsStaticAddress();                    // 0x000AEC2C
DWORD GetIPAddress();                      // 0x000AEC35
BOOL IsNetworkReady();                     // 0x000AEC3B
BOOL IsEthernetLinkUp();                   // 0x000AEC52
std::string GetIPAddressString();          // 0x000AF004
// Writes the network settings to the config area if they changed. Empty or
// invalid strings keep the current value (IP and subnet fall back to
// defaults). Returns TRUE if the settings were saved.
BOOL ApplyNetworkConfig(BOOL useDhcp, const char *ip, const char *subnet, const char *gateway,
                        const char *dns1, const char *dns2);   // 0x000AED23
// Broadcasts "ping" on UDP 4905 and collects the replies in g_netClients.
// At most once every 5 seconds. Returns the number of clients.
int ScanNetworkClients();                  // 0x000AF055
