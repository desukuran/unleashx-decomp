// What the file manager opens files in, besides the text editor (CNotePad)
// and the music player (CAudioGUI): pictures, archives browsed like
// folders and XM/XBG meshes (the FTP manager is FtpManager.h). All are
// CAppFrame screens (vtable: destructor, Init(font), IsOpen, Render,
// ProcessInput, Show).
#pragma once

#include "../app/Screens.h"
#include "../app/XBApp.h"
#include "../system/XipArchive.h"
#include "../thirdparty/Unrar.h"
#include "../render/Texture.h"
#include "FileFolder.h"

#include <string>
#include <vector>

class CFileBrowser;
class CSound;

#define IMAGEVIEWER_MAX_IMAGES 20

// The picture viewer: a bmp, jpg or png, an .xbx (one texture), or every
// texture of an .xpr (left/right step through them). START runs a
// slideshow of a picture's folder. vtable 0x0024E618, 0x29D4 bytes. See
// ImageViewer.cpp.
class CImageViewer : public CAppFrame
{
public:
	CImageViewer();                                       // 0x00118E34 (this in esi)
	virtual ~CImageViewer();                              // 0x00118DC1 (this in edi; deleting 0x00021395)
	virtual BOOL Init(CXBFontEx *font);                   // 1 (0x001185D2)
	virtual BOOL IsOpen() { return m_bOpen; }             // 2 (0x00021387)
	virtual BOOL Render();                                // 3 (0x001186F3)
	virtual HRESULT ProcessInput(int *action, SGamepadState **pad);   // 4 (0x001194AE)
	virtual void Show() { m_bOpen = TRUE; }               // 5 (0x0002138E)
	// (path in eax, this on the stack)
	BOOL Open(const char *path);                          // 0x0011935E

	enum
	{
		ZOOM_FULLSCREEN = 0,     // stretched over the screen
		ZOOM_WINDOW = 1,         // fitted in the window, with a caption
		ZOOM_ACTUAL = 2,         // its own size, shrunk to fit the screen
	};

	IDirect3DTexture8 *m_pUnknown004;                     // +0x0004 released, never set
	IDirect3DTexture8 *m_pTextures[IMAGEVIEWER_MAX_IMAGES];   // +0x0008
	DWORD m_dwTypes[IMAGEVIEWER_MAX_IMAGES];              // +0x0058 D3DResource_GetType; 3 is a texture
	wchar_t m_szNames[IMAGEVIEWER_MAX_IMAGES][0x104];     // +0x00A8
	int m_nCurrent;                                       // +0x2948
	BOOL m_bLoading;                                      // +0x294C never set
	CXBPackedResource m_xpr;                              // +0x2950 (.xpr, .xbx)
	XBRESOURCE *m_pResourceTags;                          // +0x2968 the .xpr's names, or NULL
	int m_nZoom;                                          // +0x296C ZOOM_*
	std::string m_strPath;                                // +0x2970
	BOOL m_bOpen;                                         // +0x298C
	BOOL m_bMessageShown;                                 // +0x2990
	CMessageBox *m_pMessageBox;                           // +0x2994
	CXBFontEx *m_pFont;                                   // +0x2998
	CXBFont *m_pDingsFont;                                // +0x299C XboxDings, for the slideshow sign
	BOOL m_bInitialized;                                  // +0x29A0
	float m_fY;                                           // +0x29A4
	float m_fX;                                           // +0x29A8
	float m_fWidth;                                       // +0x29AC
	float m_fHeight;                                      // +0x29B0
	DWORD m_unknown29B4;                                  // +0x29B4
	SImageInfo m_imageInfo;                               // +0x29B8 from LoadTextureFromFile
	CSlideShow *m_pSlideShow;                             // +0x29CC
	BOOL m_bSlideShowAllowed;                             // +0x29D0 a jpg, png or bmp

private:
	// (this in eax)
	HRESULT LoadImage(const char *path);                  // 0x00118EE8
	// (this in eax)
	HRESULT LoadXbx(const char *path);                    // 0x0011863F
	void ShowMessage(const char *text, const wchar_t *title, int buttons);   // 0x00119217 (this in edi)
};

// The number of images the viewer holds. 0x002761E4
extern int g_nImageCount;
// The last image index the viewer showed (written, never read). 0x0026C68C
extern int g_nLastImageIndex;

// One line of the zip browser. 0x40 bytes.
struct SZipEntry
{
	// (this in esi)
	SZipEntry(const char *name, DWORD originalSize, DWORD packedSize, int index);   // 0x00014365

	std::wstring name;                 // +0x00 without folders ("dir/" for a folder entry)
	std::wstring size;                 // +0x1C "packed/original"
	int index;                         // +0x38 in the zip
	BOOL marked;                       // +0x3C
};

// A zip's contents: mark entries (X) and extract the marked ones (or the
// highlighted one), or all, next to the zip or into a chosen folder.
// vtable 0x0024AADC, 0x2E8 bytes. See ZipManager.cpp.
class CZipManager : public CAppFrame
{
public:
	CZipManager();                                        // 0x000147A3 (this in esi)
	virtual ~CZipManager();                               // 0x0001484D (this in ebx; deleting 0x000161FB)
	virtual BOOL Init(CXBFontEx *font);                   // 1 (0x00014924)
	virtual BOOL IsOpen() { return m_bOpen; }             // 2 (0x000327ED)
	virtual BOOL Render();                                // 3 (0x00014F46)
	virtual HRESULT ProcessInput(int *action, SGamepadState **pad);   // 4 (0x000149E6)
	// Lists the zip (on a thread). (this in eax)
	BOOL Open(const char *path);                          // 0x000148C2

	enum
	{
		OP_NONE = 0,
		OP_EXTRACT_MARKED = 1,
		OP_EXTRACT_ALL = 2,
		OP_ADD = 3,              // (no menu entry leads here)
	};

	int m_nOperation;                  // +0x004
	int m_nSelected;                   // +0x008
	BOOL m_bOpen;                      // +0x00C
	CSound *m_pScrollSound;            // +0x010
	float m_fY;                        // +0x014
	float m_fX;                        // +0x018
	float m_fWidth;                    // +0x01C
	float m_fHeight;                   // +0x020
	CFileBrowser *m_pBrowser;          // +0x024 "... to..."
	COptionMenu *m_pMenu;              // +0x028
	CMessageBox *m_pMessageBox;        // +0x02C
	BOOL m_bBrowserOpen;               // +0x030
	BOOL m_bMenuOpen;                  // +0x034
	BOOL m_bMessageShown;              // +0x038
	DWORD m_unknown03C;                // +0x03C
	std::vector<SZipEntry> m_entries;  // +0x040
	std::string m_strZipFile;          // +0x050
	std::string m_strTarget;           // +0x06C where to extract
	std::string m_strAddPath;          // +0x088 what to add (OP_ADD)
	wchar_t m_szCurrentFile[0x105];    // +0x0A4 for the progress box
	int m_nTop;                        // +0x2B0
	int m_nBottom;                     // +0x2B4
	int m_nVisible;                    // +0x2B8
	BOOL m_bInitialized;               // +0x2BC
	float m_fLineHeight;               // +0x2C0
	CXBFontEx *m_pFont;                // +0x2C4
	CXBFont *m_pCaptionFont;           // +0x2C8 the dialog font, else m_pFont
	BOOL m_bDone;                      // +0x2CC the operation finished
	BOOL m_bFailed;                    // +0x2D0
	BOOL m_bRunning;                   // +0x2D4
	BOOL m_bCancel;                    // +0x2D8 B while running
	int m_nExtracted;                  // +0x2DC
	CAppSettings *m_pSettings;         // +0x2E0
	BOOL m_bListed;                    // +0x2E4

private:
	// The folder browser for "... to..." (selectFiles for OP_ADD). (this
	// in edi)
	void ShowBrowser(BOOL selectFiles);                   // 0x000155AC (shared with CXipManager)
	void ShowMessage(const char *text, const wchar_t *title, int buttons);   // 0x00017030 (this in edi; shared with CXipManager)
	void ScrollToSelection();
	static unsigned __stdcall ListThread(void *param);    // 0x0001565A
	static unsigned __stdcall ExtractThread(void *param); // 0x000158E3
};

// One line of the RAR browser. 0x58 bytes.
struct SRarEntry
{
	// (this in esi)
	SRarEntry(const char *name, const char *path, DWORD unpackedSize, DWORD packedSize);   // 0x00014579

	std::wstring name;                 // +0x00 without folders
	std::string path;                  // +0x1C in the archive
	std::wstring size;                 // +0x38 "packed/unpacked"
	BOOL marked;                       // +0x54
};

// A RAR's contents, like the zip browser (no adding). vtable 0x0024AB00,
// with an UnrarSink at +4 (vtable 0x0024AAF8) that counts the files of an
// "extract all". 0x308 bytes. See RarManager.cpp.
class CRarManager : public CAppFrame, public UnrarSink
{
public:
	CRarManager();                                        // 0x00017783 (this in esi)
	virtual ~CRarManager();                               // 0x00017854 (this in ebx; deleting 0x00018F5A)
	virtual BOOL Init(CXBFontEx *font);                   // 1 (0x0001793F)
	virtual BOOL IsOpen() { return m_bOpen; }             // 2 (0x00017F48)
	virtual BOOL Render();                                // 3 (0x00017F4C)
	virtual HRESULT ProcessInput(int *action, SGamepadState **pad);   // 4 (0x00017A01)
	// UnrarSink (thunk 0x00018975)
	virtual void OnFile(const char *name, bool ok, bool *proceed);
	// Lists the archive (on a thread). (this in eax)
	BOOL Open(const char *path);                          // 0x000178DD

	enum
	{
		OP_NONE = 0,
		OP_EXTRACT_MARKED = 1,
		OP_EXTRACT_ALL = 2,
	};

	int m_nOperation;                  // +0x008
	int m_nSelected;                   // +0x00C
	BOOL m_bOpen;                      // +0x010
	CSound *m_pScrollSound;            // +0x014
	float m_fY;                        // +0x018
	float m_fX;                        // +0x01C
	float m_fWidth;                    // +0x020
	float m_fHeight;                   // +0x024
	CFileBrowser *m_pBrowser;          // +0x028
	COptionMenu *m_pMenu;              // +0x02C
	CMessageBox *m_pMessageBox;        // +0x030
	BOOL m_bBrowserOpen;               // +0x034
	BOOL m_bMenuOpen;                  // +0x038
	BOOL m_bMessageShown;              // +0x03C
	DWORD m_unknown040;                // +0x040
	std::vector<SRarEntry> m_entries;  // +0x044
	std::string m_strRarFile;          // +0x054
	std::string m_strBaseFolder;       // +0x070 "\folder\" when the first entry is a folder
	std::string m_strTarget;           // +0x08C where to extract
	std::string m_unknown0A8;          // +0x0A8 (never used)
	wchar_t m_szCurrentFile[0x105];    // +0x0C4 for the progress box
	int m_nTop;                        // +0x2D0
	int m_nBottom;                     // +0x2D4
	int m_nVisible;                    // +0x2D8
	BOOL m_bInitialized;               // +0x2DC
	float m_fLineHeight;               // +0x2E0
	CXBFontEx *m_pFont;                // +0x2E4
	CXBFont *m_pCaptionFont;           // +0x2E8
	BOOL m_bDone;                      // +0x2EC
	BOOL m_bFailed;                    // +0x2F0
	BOOL m_bRunning;                   // +0x2F4
	int m_nExtracted;                  // +0x2F8
	BOOL m_bCancel;                    // +0x2FC B while running
	CAppSettings *m_pSettings;         // +0x300
	BOOL m_bListed;                    // +0x304

private:
	void ShowBrowser(BOOL selectFiles);                   // 0x000185AC (this in edi)
	void ShowMessage(const char *text, const wchar_t *title, int buttons);   // 0x0001865A (this in edi)
	void ScrollToSelection();
	static unsigned __stdcall ListThread(void *param);    // 0x0001877A
	static unsigned __stdcall ExtractThread(void *param); // 0x000189F7
};

// An .xip archive's contents (the MS dashboard's), like the zip browser
// (no adding). The layout matches CZipManager up to +0x2D4. vtable
// 0x0024AB28, 0x30C bytes. See XipManager.cpp.
class CXipManager : public CAppFrame
{
public:
	CXipManager();                                        // 0x00016274 (this in esi)
	virtual ~CXipManager();                               // 0x00016325 (this in ebx; deleting 0x00017767)
	virtual BOOL Init(CXBFontEx *font);                   // 1 (0x0001640F)
	virtual BOOL IsOpen() { return m_bOpen; }             // 2 (0x000327ED)
	virtual BOOL Render();                                // 3 (0x000169F0)
	virtual HRESULT ProcessInput(int *action, SGamepadState **pad);   // 4 (0x000164D1)
	// Lists the archive (on a thread). (this in eax)
	BOOL Open(const char *path);                          // 0x000163AD

	enum
	{
		OP_NONE = 0,
		OP_EXTRACT_MARKED = 1,
		OP_EXTRACT_ALL = 2,
		OP_ADD = 3,              // (checked, never set)
	};

	int m_nOperation;                  // +0x004
	int m_nSelected;                   // +0x008
	BOOL m_bOpen;                      // +0x00C
	CSound *m_pScrollSound;            // +0x010
	float m_fY;                        // +0x014
	float m_fX;                        // +0x018
	float m_fWidth;                    // +0x01C
	float m_fHeight;                   // +0x020
	CFileBrowser *m_pBrowser;          // +0x024
	COptionMenu *m_pMenu;              // +0x028
	CMessageBox *m_pMessageBox;        // +0x02C
	BOOL m_bBrowserOpen;               // +0x030
	BOOL m_bMenuOpen;                  // +0x034
	BOOL m_bMessageShown;              // +0x038
	DWORD m_unknown03C;                // +0x03C
	std::vector<SZipEntry> m_entries;  // +0x040
	std::string m_strXipFile;          // +0x050
	std::string m_strTarget;           // +0x06C
	std::string m_strAddPath;          // +0x088 (OP_ADD)
	wchar_t m_szCurrentFile[0x105];    // +0x0A4
	int m_nTop;                        // +0x2B0
	int m_nBottom;                     // +0x2B4
	int m_nVisible;                    // +0x2B8
	BOOL m_bInitialized;               // +0x2BC
	float m_fLineHeight;               // +0x2C0
	CXBFontEx *m_pFont;                // +0x2C4
	CXBFont *m_pCaptionFont;           // +0x2C8
	BOOL m_bDone;                      // +0x2CC
	BOOL m_bFailed;                    // +0x2D0
	BOOL m_bRunning;                   // +0x2D4
	CXipArchive m_archive;             // +0x2D8 open from the listing on
	int m_nExtracted;                  // +0x300
	CAppSettings *m_pSettings;         // +0x304
	BOOL m_bListed;                    // +0x308

private:
	// The same code as CZipManager's (the linker merged them).
	void ShowBrowser(BOOL selectFiles);                   // 0x000155AC (this in edi)
	void ShowMessage(const char *text, const wchar_t *title, int buttons);   // 0x00017030 (this in edi)
	void ScrollToSelection();
	static unsigned __stdcall ListThread(void *param);    // 0x00017150
	static unsigned __stdcall ExtractThread(void *param); // 0x00017397
};

// The 0x18-byte header of an .xm file: a raw mesh dump. The vertices
// (vertexSize * vertexCount bytes) and the 16-bit indices follow.
struct SXmHeader
{
	DWORD primitiveType;      // +0x00 D3DPT_*; only 1-4 (points and lines) are accepted
	DWORD primitiveCount;     // +0x04 for the indexed draw
	DWORD fvf;                // +0x08 not used TODO: verify
	DWORD vertexSize;         // +0x0C
	DWORD vertexCount;        // +0x10
	DWORD indexCount;         // +0x14
};

// The mesh viewer: an .xm (raw dump) or .xbg (XDK packed mesh, CXBMesh)
// in the middle of the screen, turned with the right stick. vtable
// 0x00258900, 0xD0 bytes. See XMViewer.cpp.
class CXMViewer : public CAppFrame
{
public:
	CXMViewer();                                          // 0x000BE4E7 (this in esi)
	virtual ~CXMViewer();                                 // 0x000BE6BB (this in eax; deleting 0x0002E101)
	virtual BOOL Init(CXBFontEx *font);                   // 1 (0x000BDF7D)
	virtual BOOL IsOpen() { return m_bOpen; }             // 2 (0x0002E0F5)
	virtual BOOL Render();                                // 3 (0x000BE52C)
	virtual HRESULT ProcessInput(int *action, SGamepadState **pad);   // 4 (0x000BE21B)
	virtual void Show() { m_bOpen = TRUE; }               // 5 (0x0002E0F9)
	// (this in ebx)
	BOOL Open(const char *path);                          // 0x000BE815

	IDirect3DVertexBuffer8 *m_pVB;     // +0x04 (.xm)
	IDirect3DIndexBuffer8 *m_pIB;      // +0x08
	SXmHeader m_header;                // +0x0C
	std::string m_strPath;             // +0x24
	BOOL m_bOpen;                      // +0x40
	BOOL m_bMessageShown;              // +0x44
	BOOL m_bStayOpen;                  // +0x48 never set: A/B/BACK always close
	CMessageBox *m_pMessageBox;        // +0x4C
	CXBFontEx *m_pFont;                // +0x50
	CXBMesh m_mesh;                    // +0x54 (.xbg)
	BOOL m_bInitialized;               // +0x6C
	float m_fUnknown70;                // +0x70 40.0 (unused)
	float m_fUnknown74;                // +0x74 30.0 (unused)
	float m_fScreenWidth;              // +0x78 (unused)
	float m_fScreenHeight;             // +0x7C (unused)
	BOOL m_bXbg;                       // +0x80 m_mesh holds the model
	DWORD m_unknown084[3];             // +0x84
	float m_matWorld[16];              // +0x90

private:
	void ReleaseModel();                                  // 0x000BE316 (this in esi)
	void ShowMessage(const char *text, const wchar_t *title, int buttons);   // 0x000BE6F8 (this in edi)
};
