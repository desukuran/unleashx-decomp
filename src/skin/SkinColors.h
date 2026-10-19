// <ColorScheme> and the colors skins default to.
#pragma once

#include "../common/Platform.h"

class CXmlDocument;

extern DWORD g_dwSkinBorderColor;   // 0x002786FC
extern DWORD g_dwSkinBackColor;     // 0x00278700
extern BOOL g_bSkinGameIcon;        // 0x00277E70 <Main><GameIcon Enable=...>

void LoadSkinColorScheme(CXmlDocument *doc);   // 0x000AD398 TODO: decompile
