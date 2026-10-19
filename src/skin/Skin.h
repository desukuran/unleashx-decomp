// Skins: Skin.xml parsing and the data the renderer draws from.
//
// A skin is a folder (or .zip) with a Skin.xml, images, fonts (.xpr bitmap
// fonts or .ttf) and optionally videos (.wmv/.xmv). Two built-in skins live
// in the XBE's INT_SKINS section. The schema is documented in
// docs/SKIN_FORMAT.md. There's one CSkin, g_skin; it has no vtable and no RTTI
// name, so "CSkin" is a reconstructed name.
#pragma once

#include <string>
#include <vector>

#include "../common/Platform.h"

class CAppSettings;
class CXBFont;
class CXBFontEx;
class CWMVideoPlayer;
class CXmlDocument;
class CXmlElement;
class CFontTable;
struct IDirect3DTexture8;

// XBFONT_* alignment flags (XDK XBFont.h), as used by <... Align="...">.
enum
{
	SKIN_ALIGN_LEFT = 0x0,
	SKIN_ALIGN_RIGHT = 0x1,
	SKIN_ALIGN_CENTER_X = 0x2,
	SKIN_ALIGN_CENTER_Y = 0x4,
	SKIN_ALIGN_TRUNCATED = 0x8,   // set when a Width is given
};

// What a <Text Source="..."> shows. Values are the original IDs.
enum SkinTextSource
{
	TEXT_DVDTITLE = 0x00,
	TEXT_C_TOTAL = 0x04, TEXT_E_TOTAL = 0x05, TEXT_F_TOTAL = 0x06,
	TEXT_C_FREE = 0x07, TEXT_E_FREE = 0x08, TEXT_F_FREE = 0x09,
	TEXT_IP = 0x0C,
	TEXT_LINKSTATUS = 0x0D,
	TEXT_TIME = 0x0E,
	TEXT_APPVERSION = 0x0F,
	TEXT_BIOSVERSION = 0x12,
	TEXT_NICKNAME = 0x14,
	TEXT_MBTEMP = 0x15,
	TEXT_CPUTEMP = 0x16,
	TEXT_DATE = 0x17,
	TEXT_DATETIME = 0x18,
	TEXT_SKINVERSION = 0x1B,
	TEXT_SKINAUTHOR = 0x1C,
	TEXT_MAC = 0x1E,
	TEXT_VERSION = 0x1F,
	TEXT_HDDKEY = 0x20,
	TEXT_HDDMODEL = 0x21,
	TEXT_HDDSERIAL = 0x22,
	TEXT_HDDSTATUS = 0x23,
	TEXT_DVDMODEL = 0x24,
	TEXT_HDTEMP = 0x26, TEXT_HDTEMPC = 0x27, TEXT_HDTEMPF = 0x28,
	TEXT_SERIALNUM = 0x29,
	TEXT_VIDEOMODE = 0x2A,
	TEXT_C_TOTALMB = 0x2B, TEXT_E_TOTALMB = 0x2C, TEXT_F_TOTALMB = 0x2D,
	TEXT_C_FREEMB = 0x2E, TEXT_E_FREEMB = 0x2F, TEXT_F_FREEMB = 0x30,
	TEXT_MBTEMPF = 0x31, TEXT_CPUTEMPF = 0x32, TEXT_MBTEMPC = 0x33, TEXT_CPUTEMPC = 0x34,
	TEXT_BIOSMD5 = 0x35,
	TEXT_TRAY = 0x38,
	TEXT_DVDTYPE = 0x39,
	TEXT_SONGNAME = 0x3A,
	TEXT_SONGTIME = 0x3B,
	TEXT_SOUNDTRACK = 0x3C,
	TEXT_NUMCLIENT = 0x3D,
	TEXT_SUBNET = 0x3E, TEXT_GATEWAY = 0x3F, TEXT_DNS1 = 0x40, TEXT_DNS2 = 0x41,
	TEXT_CHIPID = 0x42,
	TEXT_NEWSFEED = 0x43,
	TEXT_NEWSSOURCE = 0x44,
	TEXT_LAUNCHMODE = 0x45,
	TEXT_G_TOTALMB = 0x48, TEXT_G_FREEMB = 0x49, TEXT_G_TOTAL = 0x4A, TEXT_G_FREE = 0x4B,
	TEXT_C_PFREE = 0x4C, TEXT_E_PFREE = 0x4D, TEXT_F_PFREE = 0x4E, TEXT_G_PFREE = 0x4F,
	TEXT_FANSPEED = 0x50,
	TEXT_THREAD = 0x51,
	TEXT_FREEMEMORY = 0x52,
	TEXT_TOTALMEMORY = 0x53,
	TEXT_SONGBITRATE = 0x55,
	TEXT_SONGLENGTH = 0x56,
	TEXT_SONGFREQUENCY = 0x57,
	TEXT_SONGCHANNEL = 0x58,
	TEXT_SONGTRACK = 0x59,
	TEXT_STATIC = 0x5A,   // no Source: the element's own text
};

// <Shape Type="..."> / <Element Type="...">
enum SkinShapeType
{
	SHAPE_BOX = 0x01,         // default
	SHAPE_ROUNDEDBOX = 0x02,
	SHAPE_TRIANGLE = 0x03,
	SHAPE_STOPSTATUS = 0x0A,
	SHAPE_PLAYSTATUS = 0x0B,
	SHAPE_PAUSESTATUS = 0x0C,
	SHAPE_METER = 0x0D,
	SHAPE_SONGSTATUS = 0x0E,
	SHAPE_PREV = 0x14,
	SHAPE_PLAY = 0x15,
	SHAPE_PAUSE = 0x16,
	SHAPE_STOP = 0x17,
	SHAPE_NEXT = 0x18,
	SHAPE_PREVTRACK = 0x19,
	SHAPE_NEXTTRACK = 0x1A,
	SHAPE_RANDOM = 0x1B,
	SHAPE_MUTE = 0x1C,
	SHAPE_VOLUME = 0x1D,
	SHAPE_PROGRESS = 0x1E,
};

// Direction="..." of triangles, volume and progress shapes.
enum SkinDirection
{
	DIR_DEFAULT = 1,   // triangle: up; volume/progress: right
	DIR_DOWN = 2,
	DIR_LEFT = 3,
	DIR_OTHER = 4,     // triangle: Right; volume/progress: Up
};

// <Image Source="...">
enum SkinImageSource
{
	IMAGE_FILE = 0,
	IMAGE_GAMEICON = 1,
	IMAGE_GAMEPREVIEW = 2,
	IMAGE_ALBUMART = 3,
	IMAGE_DISABLED = 100,   // GamePreview without a <Preview> area
};

// One <Text>. 0x3C bytes
struct SSkinText
{
	int source;              // +0x00 SkinTextSource
	wchar_t *text;           // +0x04 static text or printf format (owned)
	int left;                // +0x08 adjusted for alignment
	int top;                 // +0x0C
	int width;               // +0x10
	int height;              // +0x14
	DWORD color;             // +0x18 ARGB, -1 = font default
	DWORD align;             // +0x1C SKIN_ALIGN_*
	int unknown20;           // +0x20 always 0
	BOOL isFormat;           // +0x24 text has a %s for the source value
	float scaleX;            // +0x28
	float scaleY;            // +0x2C
	int scroll;              // +0x30 0 none, 1 scroll, 2 ticker
	int unknown34;           // +0x34
	CXBFont *font;           // +0x38
};

// One <Shape>/<Element>. 0x24 bytes
struct SSkinShape
{
	int type;                // +0x00 SkinShapeType
	int direction;           // +0x04 SkinDirection
	int left;                // +0x08
	int top;                 // +0x0C
	int width;               // +0x10
	int height;              // +0x14
	int unknown18;           // +0x18 always 1
	DWORD borderColor;       // +0x1C
	DWORD backColor;         // +0x20
};

// One <Image>. 0x120 bytes
struct SSkinImage
{
	int left;                        // +0x00
	int top;                         // +0x04
	int width;                       // +0x08
	int height;                      // +0x0C
	IDirect3DTexture8 *texture;      // +0x10
	float opacity;                   // +0x14 0..1
	int source;                      // +0x18 SkinImageSource
	char fileName[0x104];            // +0x1C kept when the texture couldn't be loaded yet
};

// Draw-order entry for zIndex sorting. 0x0C bytes
struct SSkinDrawItem
{
	int zIndex;
	int index;               // into the shape/image/text vector
	int kind;                // 0 image, 1 shape, 2 text
};

// One <ItemPos> (fixed menu item slot). 0x14 bytes
struct SSkinItemPos
{
	int x, y, width, height;
	DWORD align;
};

struct SSkinRect
{
	int left, top, width, height;
};

#define SKIN_MAX_ITEMPOS 19

// Callbacks from CSkin (implemented by CUnleashX::CSkinEvent). The vtable
// has 6 slots; the names are reconstructed.
class CSkinEventSink
{
public:
	virtual void Unknown0();                 // slot 0
	virtual void Unknown1();                 // slot 1
	virtual void OnSkinLoaded();             // slot 2 (+0x08)
	virtual void Unknown3();                 // slot 3
	virtual void Unknown4();                 // slot 4
	virtual void OnPreviewTimer();           // slot 5 (+0x14)
};

// size 0x6BC
class CSkin
{
public:
	CSkin();                                          // TODO: find the constructor
	~CSkin();                                         // 0x000F9852

	void SetDefaults();                               // 0x000F9166
	void Free();                                      // 0x000F9372
	BOOL Parse(CXmlDocument *doc);                    // 0x000FD644
	BOOL Load(const char *path);                      // 0x000FFE1A
	// The <LoadingScreen> background, loaded on first use; NULL for
	// "GameIcon" (the launched item's icon is shown instead).
	IDirect3DTexture8 *GetLoadingBackground();        // 0x000F9B25
	// The <FileManager><BackGround> image, loaded on first use.
	IDirect3DTexture8 *GetFileManagerBackground();    // 0x000F99A7
	// Whether a preview video is playing (keeps the background sound off).
	BOOL IsPreviewPlaying() const;                    // 0x000F9145
	// The preview video between its area and full screen. (this in ecx)
	void TogglePreviewFullScreen();                   // 0x000F90E6 TODO: decompile
	// Pauses or resumes the preview video (and its sound). (playing in eax,
	// this in ecx)
	void SetPreviewPlaying(BOOL playing);             // 0x000FFDAA TODO: decompile
	// Plays file in the <Menu><Preview> area (NULL or "" stops it).
	void PlayPreview(const char *file);               // 0x000FD563 TODO: decompile
	void BeginLoad(const char *path);                 // 0x00101743
	void SetDefaultMainLayout();                      // 0x000FCEA6 TODO: decompile
	void SetDefaultMusicPlayerLayout();               // 0x000FA219 TODO: decompile

	HRESULT PollLoad(BOOL *busy);                     // 0x000F8E2B

	std::string ResolveFile(const char *path);        // 0x000F9DFC
	HRESULT FindFont(const char *name, CXBFont **font);   // 0x000F9A08

	void ParseTexts(CXmlElement *parent, const char *tag, std::vector<SSkinText> &texts,
	                std::vector<SSkinDrawItem> *order);    // 0x000FAE12
	void ParseShapes(CXmlElement *parent, const char *tag, std::vector<SSkinShape> &shapes,
	                 std::vector<SSkinDrawItem> *order);   // 0x000FC526
	void ParseImages(CXmlElement *parent, const char *tag, std::vector<SSkinImage> &images,
	                 std::vector<SSkinDrawItem> *order);   // 0x000FCB03

	BOOL m_bZipped;                     // +0x000 loaded from a .zip
	int m_unknown004;                   // +0x004
	float m_fMenuFontScaleX;            // +0x008
	float m_fMenuFontScaleY;            // +0x00C
	DWORD m_dwPreviewTick;              // +0x010
	std::string m_strTempDir;           // +0x014 where a zipped skin is extracted
	CFontTable *m_pFonts;               // +0x030 <Fonts>
	CAppSettings *m_pSettings;          // +0x034
	CSkinEventSink *m_pEventSink;       // +0x038
	std::string m_strSplashFile;        // +0x03C UXSplash.jpg/png next to the skin
	std::string m_strSkinFile;          // +0x058 path of skin.xml
	std::string m_strSkinDir;           // +0x074
	std::string m_strSkinName;          // +0x090
	wchar_t m_szName[0x50];             // +0x0AC <Skin Name=...>
	wchar_t m_szVersion[0x50];          // +0x14C <Information><Version>
	wchar_t m_szAuthor[0x50];           // +0x1EC <Information><Author>
	BOOL m_bGlobe;                      // +0x28C <Main Globe=...>
	BOOL m_bOrb;                        // +0x290 <Main Orb=...>
	BOOL m_bFog;                        // +0x294 <Main Fog=...>
	BOOL m_bColorScheme;                // +0x298 has a <ColorScheme>
	IDirect3DTexture8 *m_pBackground;   // +0x29C <Main><BackGround> image
	IDirect3DTexture8 *m_pOrbTexture;   // +0x2A0 <Main><Orb>
	IDirect3DTexture8 *m_pCellwall;     // +0x2A4 <Main><Cellwall>
	BOOL m_bLoadingShowInfo;            // +0x2A8 <LoadingScreen ShowInfo=...>
	CWMVideoPlayer *m_pBackgroundVideo; // +0x2AC <Main><BackGround> .wmv/.xmv
	CWMVideoPlayer *m_pPreviewVideo;    // +0x2B0 <Main><Menu><Preview>
	std::string m_strPreviewFile;       // +0x2B4 the preview being played
	std::string m_strIntroMovie;        // +0x2D0 <IntroMovie>
	CXBFontEx *m_pMenuFont;             // +0x2EC <Main><Menu><Font>
	CXBFont *m_pInfoFont;               // +0x2F0 <Main><InfoFont>
	BOOL m_bOwnMenuFont;                // +0x2F4 m_pMenuFont was created here
	BOOL m_bOwnInfoFont;                // +0x2F8
	BOOL m_bPreviewPlaying;             // +0x2FC
	int m_unknown300;                   // +0x300
	SSkinRect m_rcMenu;                 // +0x304 <Main><Menu><Position>
	int m_nBorderWidth;                 // +0x314 <BorderColor Width=...> 2..9
	DWORD m_dwBorderColor;              // +0x318
	DWORD m_dwBackColor;                // +0x31C
	BOOL m_bRotate;                     // +0x320
	BOOL m_bFixed;                      // +0x324 selection stays at a fixed row
	int m_nMaxItem;                     // +0x328
	int m_nFixAt;                       // +0x32C
	BOOL m_bSmooth;                     // +0x330
	int m_nAlign;                       // +0x334 0 left, 1 middle, 2 right
	BOOL m_bShowIcon;                   // +0x338
	BOOL m_bShowArrows;                 // +0x33C
	BOOL m_bPreview;                    // +0x340 has a preview area
	BOOL m_bPreviewAudio;               // +0x344
	BOOL m_bPreviewMusic;               // +0x348
	SSkinRect m_rcPreview;              // +0x34C
	BOOL m_bHorizontal;                 // +0x35C
	int m_nItemPosCount;                // +0x360
	BOOL m_bScroll;                     // +0x364
	SSkinItemPos m_itemPos[SKIN_MAX_ITEMPOS];   // +0x368
	BYTE m_unknown4CC[0x2C];            // +0x4CC
	SSkinRect m_rcGameIcon;             // +0x4F8 <Main><GameIcon><Position>
	int m_unknown508;                   // +0x508
	float m_fGameIconOpacity;           // +0x50C
	BOOL m_bFileManagerSafeBorder;      // +0x510
	BOOL m_bFileManagerDrawBorder;      // +0x514
	IDirect3DTexture8 *m_pMusicPlayerBackground;   // +0x518
	IDirect3DTexture8 *m_pPlayListBackground;      // +0x51C
	IDirect3DTexture8 *m_pButtonMap;               // +0x520
	SSkinRect m_rcMusicPlayer;          // +0x524
	SSkinRect m_rcPlayList;             // +0x534
	int m_nPlayListOffsetX;             // +0x544
	int m_nPlayListOffsetY;             // +0x548
	int m_nPlayListOffsetX2;            // +0x54C
	int m_nPlayListOffsetY2;            // +0x550
	IDirect3DTexture8 *m_pDialogBackground;        // +0x554
	BYTE m_unknown558[0x10];            // +0x558
	int m_nDialogOffsetX;               // +0x568
	int m_nDialogOffsetY;               // +0x56C
	int m_nDialogOffsetX2;              // +0x570
	int m_nDialogOffsetY2;              // +0x574
	// Index (among music-player buttons) of each button shape, 0xFF = none.
	int m_nButtonPrev, m_nButtonPlay, m_nButtonPause, m_nButtonStop, m_nButtonNext;  // +0x578
	int m_nButtonPrevTrack, m_nButtonNextTrack, m_nButtonRandom, m_nButtonMute;     // +0x58C
	BYTE m_unknown59C[8];               // +0x59C
	BOOL m_bMusicPlayerRelative;        // +0x5A4 element positions are relative to the player
	std::vector<SSkinShape> m_mainShapes;          // +0x5A8 <Main><Shape>
	std::vector<SSkinText> m_mainTexts;            // +0x5B8 <Main><Text>
	std::vector<SSkinImage> m_mainImages;          // +0x5C8 <Main><Image>
	std::vector<SSkinShape> m_musicShapes;         // +0x5D8 <MusicPlayer><Element>
	std::vector<SSkinText> m_musicTexts;           // +0x5E8 <MusicPlayer><Text>
	std::vector<SSkinImage> m_musicImages;         // +0x5F8 <MusicPlayer><Image>
	std::string m_strLoadingPreview;    // +0x608 <LoadingScreen><BackGround> video or GamePreview
	std::vector<SSkinDrawItem> m_mainOrder;        // +0x624
	BOOL m_bSortByZIndex;               // +0x634 some element had a zIndex
	std::vector<SSkinDrawItem> m_musicOrder;       // +0x638
	IDirect3DTexture8 *m_pLoadingTexture;          // +0x648
	BOOL m_bLoading;                    // +0x64C
	HANDLE m_hLoadThread;               // +0x650
	int m_unknown654;                   // +0x654
	std::string m_strLoadFile;          // +0x658
	BOOL m_bChanged;                    // +0x674
	BOOL m_bLoaded;                     // +0x678
	IDirect3DTexture8 *m_pFileManagerBackground;   // +0x67C
	std::string m_strFileManagerBackground;        // +0x680
	IDirect3DTexture8 *m_pLoadingBackground;       // +0x69C
	std::string m_strLoadingBackground;            // +0x6A0
};

extern CSkin g_skin;   // 0x00277968



// Fonts shared by every skin (MainFont, MenuFont, InfoFont, SmallFont,
// CaptionFont). 0x002784C0
extern CFontTable g_fonts;

bool CompareSkinZIndex(const SSkinDrawItem &a, const SSkinDrawItem &b);
SSkinText MakeSkinTextPublic(int left, int top, int width, int height, int source, DWORD color, BOOL isFormat,
                             int scaleX, int scaleY, int scroll, CXBFont *font, DWORD align);   // 0x000266B8
int GetInternalSkinCount();                                // 0x000F8E27
int FindInternalSkin(const char *name);                    // 0x000F90B0
DWORD ParseSkinAlign(const char *align);                   // 0x000F9B86
void ParseSkinRect(SSkinRect *rect, CXmlElement *element);  // 0x000F9CA6
