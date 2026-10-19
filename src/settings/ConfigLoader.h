// Loading Config.xml into CAppSettings.
#pragma once

#include "../common/Platform.h"

class CXmlDocument;
class CUnleashX;

extern CXmlDocument g_config;          // 0x00278160 Config.xml
extern CXmlDocument g_defaultMenu;     // 0x00278040 built-in <Menu>, see BuildDefaultMenu()
extern BOOL g_bConfigLoaded;           // 0x0026A610
extern BOOL g_bColdBoot;               // 0x00276208 TODO: verify; cleared after the first AutoSetTime
extern CUnleashX *g_pApp;              // 0x00276204

// Thread that mounts drives, loads Config.xml (or creates a default one) and
// applies it.
unsigned __stdcall ConfigLoaderThread(void *param);   // 0x000EA0A6

// Copies every known setting from g_config into g_pSettings.
void LoadConfigSettings();               // 0x000E5A4F

// Fills g_defaultMenu with the stock menu and, if Config.xml has no <Menu>,
// adds a copy of it there.
void BuildDefaultMenu();                 // 0x000E37C7
