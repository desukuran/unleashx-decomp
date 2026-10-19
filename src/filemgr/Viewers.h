// What the file manager opens files in, besides the text editor (CNotePad)
// and the music player (CAudioGUI): pictures, archives browsed like
// folders, XM/XBG modules, and the FTP manager. All are CAppFrame screens
// (vtable: destructor, Init(font), IsOpen, Render, ProcessInput, Show).
// Only the interface the file manager uses is declared. TODO: decompile
#pragma once

#include "../app/Screens.h"
#include "FileFolder.h"

// A picture (bmp jpg png xbx xpr) full screen, with a slideshow of its
// folder. vtable 0x0024E618, 0x29D4 bytes.
class CImageViewer : public CAppFrame
{
public:
	CImageViewer();                                       // 0x00118E34
	virtual ~CImageViewer();                              // 0x00118DC1 (deleting 0x00021395)
	virtual BOOL Init(CXBFontEx *font);                   // 1 (0x001185D2)
	virtual BOOL IsOpen();                                // 2 (0x00021387)
	virtual BOOL Render();                                // 3 (0x001186F3)
	virtual HRESULT ProcessInput(int *action, SGamepadState **pad);   // 4 (0x001194AE)
	virtual void Show();                                  // 5 (0x0002138E)
	// (path in eax)
	BOOL Open(const char *path);                          // 0x0011935E
};

// A zip's contents, with extracting. vtable 0x0024AADC, 0x2E8 bytes.
class CZipManager : public CAppFrame
{
public:
	CZipManager();                                        // 0x000147A3
	virtual ~CZipManager();                               // 0x0001484D (deleting 0x000161FB)
	virtual BOOL Init(CXBFontEx *font);                   // 1 (0x00014924)
	virtual BOOL IsOpen();                                // 2 (0x000327ED)
	virtual BOOL Render();                                // 3 (0x00014F46)
	virtual HRESULT ProcessInput(int *action, SGamepadState **pad);   // 4 (0x000149E6)
	// (this in eax)
	BOOL Open(const char *path);                          // 0x000148C2
};

// A RAR's contents. vtable 0x0024AB00 (an UnrarSink at +4), 0x308 bytes.
class CRarManager : public CAppFrame
{
public:
	CRarManager();                                        // 0x00017783
	virtual ~CRarManager();                               // 0x00017854 (deleting 0x00018F5A)
	virtual BOOL Init(CXBFontEx *font);                   // 1 (0x0001793F)
	virtual BOOL IsOpen();                                // 2 (0x00017F48)
	virtual BOOL Render();                                // 3 (0x00017F4C)
	virtual HRESULT ProcessInput(int *action, SGamepadState **pad);   // 4 (0x00017A01)
	// (this in eax)
	BOOL Open(const char *path);                          // 0x000178DD
};

// An .xip archive (the MS dashboard's). vtable 0x0024AB28, 0x30C bytes.
class CXipManager : public CAppFrame
{
public:
	CXipManager();                                        // 0x00016274
	virtual ~CXipManager();                               // 0x00016325 (deleting 0x00017767)
	virtual BOOL Init(CXBFontEx *font);                   // 1 (0x0001640F)
	virtual BOOL IsOpen();                                // 2 (0x000327ED)
	virtual BOOL Render();                                // 3 (0x000169F0)
	virtual HRESULT ProcessInput(int *action, SGamepadState **pad);   // 4 (0x000164D1)
	// (this in eax)
	BOOL Open(const char *path);                          // 0x000163AD
};

// Plays an XM module (and .xbg). vtable 0x00258900, 0xD0 bytes.
class CXMViewer : public CAppFrame
{
public:
	CXMViewer();                                          // 0x000BE4E7
	virtual ~CXMViewer();                                 // 0x000BE6BB (deleting 0x0002E101)
	virtual BOOL Init(CXBFontEx *font);                   // 1 (0x000BDF7D)
	virtual BOOL IsOpen();                                // 2 (0x0002E0F5)
	virtual BOOL Render();                                // 3 (0x000BE52C)
	virtual HRESULT ProcessInput(int *action, SGamepadState **pad);   // 4 (0x000BE21B)
	virtual void Show();                                  // 5 (0x0002E0F9)
	// (this in ebx)
	BOOL Open(const char *path);                          // 0x000BE815
};

// The FTP manager: the site list and two panes, local and FTP
// (CFTPFileFolder). vtable 0x0024B6E4, 0x750 bytes.
class CFTPManager : public CAppFrame
{
public:
	CFTPManager();                                        // 0x00123641 (this in ebx)
	virtual ~CFTPManager();                               // 0x001213F8 (deleting 0x0001EF3D)
	virtual BOOL Init(CXBFontEx *font);                   // 1 (0x0011F187)
	virtual BOOL IsOpen();                                // 2 (0x0001E86B)
	virtual BOOL Render();                                // 3 (0x00121C85)
	virtual HRESULT ProcessInput(int *action, SGamepadState **pad);   // 4 (0x00124A37)
	virtual void Show();                                  // 5 (0x0011F5A2)
	// The file manager's font, background and left pane (its look and
	// starting folder). (background in eax, local in ecx)
	void Setup(CXBFontEx *font, IDirect3DTexture8 *background, IFileFolder *local);   // 0x001219B8

	BYTE m_reserved004[0x734 - 4];
	BOOL m_bDrawBorder;                                   // +0x734
	BOOL m_bSafeBorder;                                   // +0x738
	BYTE m_reserved73C[0x750 - 0x73C];
};
