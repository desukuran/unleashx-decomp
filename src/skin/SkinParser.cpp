// Skin.xml parsing (CSkin::Parse and the element parsers).
#include "Skin.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>

#include "../common/FileUtil.h"
#include "../common/StrUtil.h"
#include "../media/WMVideoPlayer.h"
#include "../render/Fonts.h"
#include "../render/Texture.h"
#include "../settings/AppSettings.h"
#include "../xml/XmlDocument.h"
#include "SkinColors.h"

// Opacity and Scale attributes are percentages.
static const float kPercent = 0.01f;

static long AttrLong(CXmlAttribute *attr)
{
	return atol(attr->m_strValue.c_str());
}

static DWORD ParseHex(const std::string &s)
{
	char *end;
	return strtoul(s.c_str(), &end, 16);
}

static BOOL EndsWithVideo(const std::string &path)
{
	return StrCmpNoCase(StrRight(path, 4), ".wmv") == 0 || StrCmpNoCase(StrRight(path, 4), ".xmv") == 0;
}

// 0x000F9B86
DWORD ParseSkinAlign(const char *align)
{
	if (align == NULL)
		return SKIN_ALIGN_LEFT;
	std::string s(align);
	if (StrCmpNoCase(s.c_str(), "TopLeft") == 0)
		return SKIN_ALIGN_LEFT;
	if (StrCmpNoCase(s.c_str(), "TopCenter") == 0 || StrCmpNoCase(s.c_str(), "Middle") == 0)
		return SKIN_ALIGN_CENTER_X;
	if (StrCmpNoCase(s.c_str(), "TopRight") == 0 || StrCmpNoCase(s.c_str(), "Right") == 0)
		return SKIN_ALIGN_RIGHT;
	if (StrCmpNoCase(s.c_str(), "MiddleLeft") == 0)
		return SKIN_ALIGN_CENTER_Y;
	if (StrCmpNoCase(s.c_str(), "MiddleCenter") == 0)
		return SKIN_ALIGN_CENTER_Y | SKIN_ALIGN_CENTER_X;
	if (StrCmpNoCase(s.c_str(), "MiddleRight") == 0)
		return SKIN_ALIGN_CENTER_Y | SKIN_ALIGN_RIGHT;
	return SKIN_ALIGN_LEFT;
}

// 0x000F9CA6
// Left/Top/Width/Height attributes; zero values are ignored.
void ParseSkinRect(SSkinRect *rect, CXmlElement *element)
{
	if (element == NULL)
		return;
	for (std::vector<CXmlAttribute *>::iterator it = element->m_attributes.begin();
	     it != element->m_attributes.end(); ++it)
	{
		CXmlAttribute *attr = *it;
		if (attr == NULL)
			continue;
		long value = AttrLong(attr);
		if (value == 0)
			continue;
		if (StrCmpNoCase(attr->m_strName, "Top") == 0)
			rect->top = value;
		else if (StrCmpNoCase(attr->m_strName, "Left") == 0)
			rect->left = value;
		else if (StrCmpNoCase(attr->m_strName, "Width") == 0)
			rect->width = value;
		else if (StrCmpNoCase(attr->m_strName, "Height") == 0)
			rect->height = value;
	}
}

// 0x000F9A08
// The standard font names (case-insensitive) come from the global table;
// anything else from the skin's own <Fonts>.
HRESULT CSkin::FindFont(const char *name, CXBFont **font)
{
	if (font == NULL)
		return E_FAIL;
	*font = NULL;

	std::string key = StrToLower(name);
	const char *standard = NULL;
	if (key.compare("menufont") == 0)
		standard = "MenuFont";
	else if (key.compare("smallfont") == 0)
		standard = "SmallFont";
	else if (key.compare("infofont") == 0 || key.compare("infotextfont") == 0)
		standard = "InfoFont";
	else if (key.compare("captionfont") == 0)
		standard = "CaptionFont";
	else if (key.compare("defaultfont") == 0 || key.compare("mainfont") == 0)
		standard = "MainFont";

	if (standard != NULL)
		return g_fonts.Find(standard, font);
	if (m_pFonts == NULL)
		return E_FAIL;
	return m_pFonts->Find(name, font);
}

// 0x000F9DFC
// Like ResolvePath(), but relative paths are relative to the skin's
// directory and ".\" to the Skins folder. When running from DVD a "D:" path
// is used exactly as written.
std::string CSkin::ResolveFile(const char *path)
{
	std::string base;
	std::string out = StrToLower(path);  // _strdup + _mbslwr, leaked
	CAppSettings *settings = m_pSettings;

	if (out.size() == 0)
		out = settings->m_strHomePath;
	StrReplaceAll(out, "/", "\\");

	if (settings->m_bRunFromDVD)
	{
		if (StrLeft(out, 2).compare("d:") == 0 || StrLeft(out, 2).compare("D:") == 0)
			return std::string(path);
	}
	else
	{
		StrReplaceAll(out, "D:\\", settings->m_strHomePath.c_str());
		StrReplaceAll(out, "d:\\", settings->m_strHomePath.c_str());
	}

	if (StrFind(out, ":\\") == -1)
	{
		if (StrLeft(out, 1).compare("\\") == 0)
		{
			base = StrLeft(settings->m_strHomePath, 2);
		}
		else if (StrLeft(out, 2).compare(".\\") != 0)
		{
			// Relative to the skin directory; "..\" goes up a level each.
			base = m_strSkinDir;
			if (StrFind(out, "..\\") != -1)
			{
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
			StrRemoveChar(out, '"');
			return out;
		}
		else
		{
			ResolvePath(settings->m_prefs.m_strSkinPath.c_str(), base);
			StrReplaceAll(out, ".\\", "");
		}
		base += out;
		out = base.c_str();
		base.clear();
	}

	StrRemoveChar(out, '"');
	return out;
}

// Maps a Source keyword (lowercased) to its ID; -1 if unknown.
static int LookupTextSource(const std::string &key)
{
	static const struct
	{
		const char *name;
		int id;
	} kSources[] = {
		{"dvdtitle", TEXT_DVDTITLE}, {"c_free", TEXT_C_FREE}, {"c_total", TEXT_C_TOTAL},
		{"e_free", TEXT_E_FREE}, {"e_total", TEXT_E_TOTAL}, {"f_free", TEXT_F_FREE},
		{"f_total", TEXT_F_TOTAL}, {"g_free", TEXT_G_FREE}, {"g_total", TEXT_G_TOTAL},
		{"c_freemb", TEXT_C_FREEMB}, {"c_totalmb", TEXT_C_TOTALMB}, {"e_freemb", TEXT_E_FREEMB},
		{"e_totalmb", TEXT_E_TOTALMB}, {"f_freemb", TEXT_F_FREEMB}, {"f_totalmb", TEXT_F_TOTALMB},
		{"g_freemb", TEXT_G_FREEMB}, {"g_totalmb", TEXT_G_TOTALMB}, {"time", TEXT_TIME},
		{"date", TEXT_DATE}, {"datetime", TEXT_DATETIME}, {"nickname", TEXT_NICKNAME},
		{"cputemp", TEXT_CPUTEMP}, {"mbtemp", TEXT_MBTEMP}, {"cputempc", TEXT_CPUTEMPC},
		{"mbtempc", TEXT_MBTEMPC}, {"cputempf", TEXT_CPUTEMPF}, {"mbtempf", TEXT_MBTEMPF},
		{"songname", TEXT_SONGNAME}, {"soundtrack", TEXT_SOUNDTRACK}, {"songtime", TEXT_SONGTIME},
		{"tray", TEXT_TRAY}, {"dvdtype", TEXT_DVDTYPE}, {"videomode", TEXT_VIDEOMODE},
		{"ip", TEXT_IP}, {"subnet", TEXT_SUBNET}, {"gateway", TEXT_GATEWAY}, {"dns1", TEXT_DNS1},
		{"dns2", TEXT_DNS2}, {"linkstatus", TEXT_LINKSTATUS}, {"appversion", TEXT_APPVERSION},
		{"mac", TEXT_MAC}, {"skinauthor", TEXT_SKINAUTHOR}, {"skinversion", TEXT_SKINVERSION},
		{"version", TEXT_VERSION}, {"hddkey", TEXT_HDDKEY}, {"serialnum", TEXT_SERIALNUM},
		{"biosversion", TEXT_BIOSVERSION}, {"biosmd5", TEXT_BIOSMD5}, {"numclient", TEXT_NUMCLIENT},
		{"chipid", TEXT_CHIPID}, {"newsfeed", TEXT_NEWSFEED}, {"newssource", TEXT_NEWSSOURCE},
		{"launchmode", TEXT_LAUNCHMODE}, {"fanspeed", TEXT_FANSPEED}, {"thread", TEXT_THREAD},
		{"hddmodel", TEXT_HDDMODEL}, {"hddserial", TEXT_HDDSERIAL}, {"hddstatus", TEXT_HDDSTATUS},
		{"dvdmodel", TEXT_DVDMODEL}, {"freememory", TEXT_FREEMEMORY}, {"totalmemory", TEXT_TOTALMEMORY},
		{"hdtemp", TEXT_HDTEMP}, {"hdtempc", TEXT_HDTEMPC}, {"hdtempf", TEXT_HDTEMPF},
		{"songbitrate", TEXT_SONGBITRATE}, {"songlength", TEXT_SONGLENGTH},
		{"songfrequency", TEXT_SONGFREQUENCY}, {"songchannel", TEXT_SONGCHANNEL},
		{"songtrack", TEXT_SONGTRACK}, {"c_pfree", TEXT_C_PFREE}, {"e_pfree", TEXT_E_PFREE},
		{"f_pfree", TEXT_F_PFREE}, {"g_pfree", TEXT_G_PFREE},
	};
	for (size_t i = 0; i < sizeof(kSources) / sizeof(kSources[0]); i++)
		if (key.compare(kSources[i].name) == 0)
			return kSources[i].id;
	return -1;
}

// Builds a text record. 0x000266B8
static SSkinText MakeSkinText(int left, int top, int width, int height, int source, const std::string &text,
                              DWORD color, int unknown20, BOOL isFormat, int scaleX, int scaleY, int scroll,
                              CXBFont *font, DWORD align)
{
	SSkinText t;
	t.source = source;
	t.color = color;
	t.unknown20 = unknown20;
	t.isFormat = isFormat;
	t.scroll = scroll;
	t.font = font;
	t.top = top;
	t.left = left;
	t.align = align;
	t.unknown34 = 0;
	t.height = height;
	t.width = width;

	if (width == 0)
	{
		t.scroll = 0;
	}
	else
	{
		t.align |= SKIN_ALIGN_TRUNCATED;
		float screenWidth;
		GetDisplaySize(&screenWidth, NULL, FALSE);
		if (screenWidth < (float)(unsigned)(width + left))
			width = (int)screenWidth - left;
	}
	if (t.scroll != 0 && t.scroll == 2)
		t.align &= ~(SKIN_ALIGN_RIGHT | SKIN_ALIGN_TRUNCATED | 0x10);

	// Apply the alignment to the anchor point.
	if (t.align & SKIN_ALIGN_RIGHT)
		t.left += width;
	else if (t.align & SKIN_ALIGN_CENTER_X)
		t.left += width >> 1;
	if (height != 0 && (t.align & SKIN_ALIGN_CENTER_Y))
		t.top += height >> 1;

	t.scaleY = 1.0f;
	t.scaleX = (float)(unsigned)scaleX * kPercent;
	if (t.scaleX <= 0.0f)
		t.scaleX = 1.0f;
	t.scaleY = (float)(unsigned)scaleY * kPercent;
	if (t.scaleY <= 0.0f)
		t.scaleY = 1.0f;

	t.text = NULL;
	if ((source == TEXT_STATIC || t.isFormat) && text.size() != 0)
	{
		t.text = new wchar_t[text.size() + 1];
		if (t.text != NULL)
		{
			// The original widens through the locale's codecvt.
			for (size_t i = 0; i <= text.size(); i++)
				t.text[i] = (unsigned char)text.c_str()[i];
		}
	}
	else
	{
		t.isFormat = FALSE;
	}

	// Hard-coded clamp to 640x480.
	if (t.width > 640)
		t.width = 640;
	if (t.height > 480)
		t.height = 480;
	return t;
}

// 0x000FAE12
// <Text Source="..." Left Top Width Height Align Color Scroll Font zIndex ScaleX ScaleY>text</Text>
// Source is a keyword ("time"), text containing one "$Keyword$" (which
// becomes a %s for the value), or plain text to show as is. The element's own
// content is ignored.
void CSkin::ParseTexts(CXmlElement *parent, const char *tag, std::vector<SSkinText> &texts,
                       std::vector<SSkinDrawItem> *order)
{
	// Tested in this order; the first one found wins. Each maps to the ID of
	// the same keyword without the '$'s.
	static const char *const kTokens[] = {
		"$DVDTitle$", "$C_Free$", "$C_Total$", "$E_Free$", "$E_Total$", "$F_Free$", "$F_Total$", "$G_Free$",
		"$G_Total$", "$C_FreeMB$", "$C_TotalMB$", "$E_FreeMB$", "$E_TotalMB$", "$F_FreeMB$", "$F_TotalMB$",
		"$G_FreeMB$", "$G_TotalMB$", "$DateTime$", "$Time$", "$Date$", "$NickName$", "$CPUTemp$", "$MBTemp$",
		"$CPUTempC$", "$MBTempC$", "$CPUTempF$", "$MBTempF$", "$SongName$", "$SoundTrack$", "$SongTime$", "$Tray$",
		"$DVDType$", "$VideoMode$", "$IP$", "$Subnet$", "$Gateway$", "$DNS1$", "$DNS2$", "$LinkStatus$",
		"$AppVersion$", "$MAC$", "$SkinAuthor$", "$SkinVersion$", "$Version$", "$HDDKey$", "$SerialNum$",
		"$BIOSVersion$", "$BIOSMD5$", "$NumClient$", "$ChipID$", "$Newsfeed$", "$NewsSource$", "$LaunchMode$",
		"$FanSpeed$", "$Thread$", "$HDDModel$", "$HDDSerial$", "$HDDStatus$", "$DVDModel$", "$FreeMemory$",
		"$TotalMemory$", "$HDTemp$", "$HDTempC$", "$HDTempF$", "$SongBitRate$", "$SongLength$", "$SongFrequency$",
		"$SongChannel$", "$SongTrack$", "$C_PFree$", "$E_PFree$", "$F_PFree$", "$G_PFree$",
	};

	if (parent == NULL || tag == NULL)
		return;

	std::string source, key;
	for (CXmlElement *element = parent->FindFirstChild(tag); element != NULL; element = parent->FindNextChild())
	{
		DWORD color = 0xFFFFFFFF;
		int top = 0, left = 0, width = 0, height = 0;
		DWORD align = SKIN_ALIGN_LEFT;
		int id = TEXT_STATIC;
		BOOL isFormat = FALSE;
		int scroll = 0;
		int scaleX = 100, scaleY = 100;
		int zIndex = 0;
		CXBFont *font = NULL;

		source = element->GetAttribute("Source");
		if (source.size() != 0)
		{
			key = StrToLower(source);
			int found = LookupTextSource(key);
			if (found != -1)
			{
				id = found;
			}
			else if (StrFind(source, "$") != -1)
			{
				StrRemoveChar(source, '%');  // so the text can't inject printf formats
				isFormat = TRUE;
				for (size_t i = 0; i < sizeof(kTokens) / sizeof(kTokens[0]); i++)
				{
					if (StrFind(source, kTokens[i]) != -1)
					{
						StrReplaceAll(source, kTokens[i], "%s");
						std::string token(kTokens[i] + 1, strlen(kTokens[i]) - 2);
						id = LookupTextSource(StrToLower(token));
						break;
					}
				}
			}
		}

		for (std::vector<CXmlAttribute *>::iterator it = element->m_attributes.begin();
		     it != element->m_attributes.end(); ++it)
		{
			CXmlAttribute *attr = *it;
			if (attr == NULL)
				continue;
			unsigned long value = (unsigned long)AttrLong(attr);
			const std::string &name = attr->m_strName;

			if (StrCmpNoCase(name, "Top") == 0)
				top = value;
			else if (StrCmpNoCase(name, "Left") == 0)
				left = value;
			else if (StrCmpNoCase(name, "Width") == 0)
				width = value;
			else if (StrCmpNoCase(name, "Height") == 0)
				height = value;
			else if (value >= 25 && StrCmpNoCase(name, "ScaleX") == 0)
				scaleX = value;   // quirk: ScaleX/ScaleY below 25 are ignored
			else if (value >= 25 && StrCmpNoCase(name, "ScaleY") == 0)
				scaleY = value;
			else if (StrCmpNoCase(name, "Align") == 0)   // the rest only for values < 25
				align = ParseSkinAlign(attr->m_strValue.c_str());
			else if (StrCmpNoCase(name, "Color") == 0)
			{
				DWORD c = ParseHex(attr->m_strValue);
				if (c != 0)
					color = c;
			}
			else if (StrCmpNoCase(name, "Scroll") == 0)
			{
				if (attr->m_strValue.size() != 0)
					scroll = IsTrueString(attr->m_strValue.c_str()) ? 1 : atol(attr->m_strValue.c_str());
			}
			else if (StrCmpNoCase(name, "Font") == 0)
			{
				if (attr->m_strValue.size() != 0)
					FindFont(attr->m_strValue.c_str(), &font);
			}
			else if (value != 0 && StrCmpNoCase(name, "zIndex") == 0)
			{
				zIndex = value;
				m_bSortByZIndex = TRUE;
			}
		}

		// A continuous newsfeed ticker always scrolls as a ticker.
		if (m_pSettings->m_bNewsfeedContinuous && id == TEXT_NEWSFEED)
		{
			scroll = 2;
			isFormat = FALSE;
		}
		if (font == NULL)
			font = m_pInfoFont;

		texts.push_back(MakeSkinText(left, top, width, height, id, source, color, 0, isFormat, scaleX, scaleY,
		                             scroll, font, align));
		if (order != NULL)
		{
			SSkinDrawItem item = { zIndex, (int)texts.size() - 1, 2 };
			order->push_back(item);
		}
	}
}

// 0x0002667F (shape in eax)
static SSkinShape MakeSkinShape(int type, int left, int top, int width, int height, DWORD borderColor,
                                DWORD backColor, int unknown18, int direction)
{
	SSkinShape shape;
	shape.type = type;
	shape.direction = direction;
	shape.unknown18 = unknown18;
	shape.borderColor = borderColor;
	shape.backColor = backColor;
	shape.top = top;
	shape.left = left;
	shape.height = height;
	shape.width = width;
	return shape;
}

// 0x000FC526
// <Shape Type="box|roundedbox|triangle|meter|volume|progress|play|..." Direction
//        Left Top Width Height BackColor BorderColor zIndex/>
void CSkin::ParseShapes(CXmlElement *parent, const char *tag, std::vector<SSkinShape> &shapes,
                        std::vector<SSkinDrawItem> *order)
{
	if (parent == NULL || tag == NULL)
		return;

	struct ButtonType
	{
		const char *name;
		int type;
		int CSkin::*slot;
	};
	static const ButtonType kButtons[] = {
		{"prev", SHAPE_PREV, &CSkin::m_nButtonPrev},
		{"play", SHAPE_PLAY, &CSkin::m_nButtonPlay},
		{"pause", SHAPE_PAUSE, &CSkin::m_nButtonPause},
		{"stop", SHAPE_STOP, &CSkin::m_nButtonStop},
		{"next", SHAPE_NEXT, &CSkin::m_nButtonNext},
		{"prevtrack", SHAPE_PREVTRACK, &CSkin::m_nButtonPrevTrack},
		{"nexttrack", SHAPE_NEXTTRACK, &CSkin::m_nButtonNextTrack},
		{"random", SHAPE_RANDOM, &CSkin::m_nButtonRandom},
		{"mute", SHAPE_MUTE, &CSkin::m_nButtonMute},
	};

	std::string type;
	int buttonIndex = 0;   // counts button shapes only
	for (CXmlElement *element = parent->FindFirstChild(tag); element != NULL; element = parent->FindNextChild())
	{
		int top = 0, left = 0, width = 0, height = 0;
		int shapeType = SHAPE_BOX;
		int direction = DIR_DEFAULT;
		DWORD borderColor = 0, backColor = 0;
		int zIndex = 0;

		type = StrToLower(element->GetAttribute("Type"));
		if (type.compare("triangle") == 0)
		{
			shapeType = SHAPE_TRIANGLE;
			std::string dir = element->GetAttribute("Direction");
			if (StrCmpNoCase(dir.c_str(), "Down") == 0)
				direction = DIR_DOWN;
			else if (StrCmpNoCase(dir.c_str(), "Left") == 0)
				direction = DIR_LEFT;
			else if (StrCmpNoCase(dir.c_str(), "Right") == 0)
				direction = DIR_OTHER;
		}
		else if (type.compare("roundedbox") == 0)
			shapeType = SHAPE_ROUNDEDBOX;
		else if (type.compare("stopstatus") == 0)
			shapeType = SHAPE_STOPSTATUS;
		else if (type.compare("playstatus") == 0)
			shapeType = SHAPE_PLAYSTATUS;
		else if (type.compare("pausestatus") == 0)
			shapeType = SHAPE_PAUSESTATUS;
		else if (type.compare("songstatus") == 0)
			shapeType = SHAPE_SONGSTATUS;
		else if (type.compare("meter") == 0)
			shapeType = SHAPE_METER;
		else if (type.compare("volume") == 0 || type.compare("progress") == 0)
		{
			shapeType = type.compare("volume") == 0 ? SHAPE_VOLUME : SHAPE_PROGRESS;
			std::string dir = element->GetAttribute("Direction");
			if (StrCmpNoCase(dir.c_str(), "Down") == 0)
				direction = DIR_DOWN;
			else if (StrCmpNoCase(dir.c_str(), "Left") == 0)
				direction = DIR_LEFT;
			else if (StrCmpNoCase(dir.c_str(), "Up") == 0)
				direction = DIR_OTHER;
		}
		else
		{
			for (size_t i = 0; i < sizeof(kButtons) / sizeof(kButtons[0]); i++)
			{
				if (type.compare(kButtons[i].name) == 0)
				{
					shapeType = kButtons[i].type;
					this->*kButtons[i].slot = buttonIndex;
					buttonIndex++;
					break;
				}
			}
		}

		for (std::vector<CXmlAttribute *>::iterator it = element->m_attributes.begin();
		     it != element->m_attributes.end(); ++it)
		{
			CXmlAttribute *attr = *it;
			if (attr == NULL)
				continue;
			long value = AttrLong(attr);
			if (value != 0)
			{
				if (StrCmpNoCase(attr->m_strName, "Top") == 0)
					top = value;
				else if (StrCmpNoCase(attr->m_strName, "Left") == 0)
					left = value;
				else if (StrCmpNoCase(attr->m_strName, "Width") == 0)
					width = value;
				else if (StrCmpNoCase(attr->m_strName, "Height") == 0)
					height = value;
				else if (StrCmpNoCase(attr->m_strName, "zIndex") == 0)
				{
					m_bSortByZIndex = TRUE;
					zIndex = value;
				}
			}
			else
			{
				// Colors are hex ("0xFF404040"), which atol() reads as 0.
				value = (long)ParseHex(attr->m_strValue);
				if (value != 0)
				{
					if (StrCmpNoCase(attr->m_strName, "BackColor") == 0)
						backColor = value;
					else if (StrCmpNoCase(attr->m_strName, "BorderColor") == 0)
						borderColor = value;
				}
			}
		}

		shapes.push_back(MakeSkinShape(shapeType, left, top, width, height, borderColor, backColor, 1, direction));
		if (order != NULL)
		{
			SSkinDrawItem item = { zIndex, (int)shapes.size() - 1, 1 };
			order->push_back(item);
		}
	}
}

// Builds an image record. 0x0002692D
static SSkinImage MakeSkinImage(IDirect3DTexture8 *texture, int left, int top, int width, int height,
                                unsigned int opacity, const char *fileName, int source)
{
	SSkinImage image;
	memset(&image, 0, sizeof(image));
	image.source = source;
	image.texture = NULL;
	if (texture != NULL)
		image.texture = texture;
	else if (source == IMAGE_FILE && fileName != NULL)
		strcpy(image.fileName, fileName);

	image.top = top;
	image.left = left;
	image.height = height;
	image.width = width;
	image.opacity = 1.0f;
	if (opacity > 1)
		image.opacity = (opacity >= 100 ? 100.0f : (float)opacity) * kPercent;
	return image;
}

// 0x000FCB03
// <Image Source="GameIcon|GamePreview|AlbumArt" Left Top Width Height Opacity zIndex>file</Image>
void CSkin::ParseImages(CXmlElement *parent, const char *tag, std::vector<SSkinImage> &images,
                        std::vector<SSkinDrawItem> *order)
{
	if (parent == NULL || tag == NULL)
		return;

	std::string source;
	for (CXmlElement *element = parent->FindFirstChild(tag); element != NULL; element = parent->FindNextChild())
	{
		int top = 0, left = 0, width = 0, height = 0, zIndex = 0;
		unsigned int opacity = 1;
		int kind = IMAGE_FILE;

		source = element->GetAttribute("Source");
		BOOL haveSource = FALSE;
		if (source.size() != 0)
		{
			if (StrCmpNoCase(source.c_str(), "GameIcon") == 0)
				kind = IMAGE_GAMEICON;
			if (StrCmpNoCase(source.c_str(), "AlbumArt") == 0)
				kind = IMAGE_ALBUMART;
			else if (StrCmpNoCase(source.c_str(), "GamePreview") == 0)
				kind = m_bPreview ? IMAGE_GAMEPREVIEW : IMAGE_DISABLED;
			haveSource = kind != IMAGE_FILE;
		}
		if (!haveSource)
		{
			// A file name: the element's text, else the Source attribute.
			source = element->GetValue();
			source = StrTrimRight(StrTrimLeft(source));
			if (source.size() == 0)
				source = element->GetAttribute("Source");
			if (source.size() == 0)
				continue;
		}

		IDirect3DTexture8 *texture = NULL;
		for (std::vector<CXmlAttribute *>::iterator it = element->m_attributes.begin();
		     it != element->m_attributes.end(); ++it)
		{
			CXmlAttribute *attr = *it;
			if (attr == NULL)
				continue;
			long value = AttrLong(attr);
			if (value == 0)
				continue;
			if (StrCmpNoCase(attr->m_strName, "Top") == 0)
				top = value;
			else if (StrCmpNoCase(attr->m_strName, "Left") == 0)
				left = value;
			else if (StrCmpNoCase(attr->m_strName, "Width") == 0)
				width = value;
			else if (StrCmpNoCase(attr->m_strName, "Height") == 0)
				height = value;
			else if (StrCmpNoCase(attr->m_strName, "Opacity") == 0)
				opacity = value;
			else if (StrCmpNoCase(attr->m_strName, "zIndex") == 0)
			{
				m_bSortByZIndex = TRUE;
				zIndex = value;
			}
		}

		if (kind == IMAGE_FILE)
		{
			source = ResolveFile(source.c_str());
			SImageInfo info;
			memset(&info, 0, sizeof(info));
			if (!LoadTextureFromFile(source.c_str(), g_pd3dDevice, &texture, width, height, &info))
			{
				// Kept as a file name; U:\ is the dashboard's UDATA folder.
				texture = NULL;
				StrReplaceAll(source, "E:\\UDATA\\9e115330\\", "U:\\");
			}
			else if (info.width != 0)
			{
				if (width == 0)
					width = info.width;
				if (height == 0)
					height = info.height;
			}
		}

		images.push_back(MakeSkinImage(texture, left, top, width == 0 ? 1 : width, height == 0 ? 1 : height,
		                               opacity, source.c_str(), kind));
		if (order != NULL)
		{
			SSkinDrawItem item = { zIndex, (int)images.size() - 1, 0 };
			order->push_back(item);
		}
	}
}

// 0x000F8EC6 (the items come by value)
bool CompareSkinZIndex(const SSkinDrawItem &a, const SSkinDrawItem &b)
{
	return a.zIndex < b.zIndex;
}

SSkinText MakeSkinTextPublic(int left, int top, int width, int height, int source, DWORD color, BOOL isFormat,
                             int scaleX, int scaleY, int scroll, CXBFont *font, DWORD align)
{
	return MakeSkinText(left, top, width, height, source, std::string(""), color, 0, isFormat, scaleX, scaleY,
	                    scroll, font, align);
}

// Reads an element's text and resolves it as a skin file; FALSE if empty.
static BOOL GetSkinFile(CSkin *skin, CXmlElement *element, std::string &path)
{
	if (element == NULL || element->m_strValue.size() == 0)
		return FALSE;
	path = skin->ResolveFile(element->GetValue().c_str());
	return TRUE;
}

// <... OffsetX OffsetY OffsetX2 OffsetY2> (non-zero values only)
static void ParseOffsets(CXmlElement *element, int &x, int &y, int &x2, int &y2)
{
	for (std::vector<CXmlAttribute *>::iterator it = element->m_attributes.begin();
	     it != element->m_attributes.end(); ++it)
	{
		CXmlAttribute *attr = *it;
		if (attr == NULL)
			continue;
		long value = AttrLong(attr);
		if (value == 0)
			continue;
		if (StrCmpNoCase(attr->m_strName, "OffsetX") == 0)
			x = value;
		else if (StrCmpNoCase(attr->m_strName, "OffsetY") == 0)
			y = value;
		else if (StrCmpNoCase(attr->m_strName, "OffsetX2") == 0)
			x2 = value;
		else if (StrCmpNoCase(attr->m_strName, "OffsetY2") == 0)
			y2 = value;
	}
	if (x2 == 0)
		x2 = x;
	if (y2 == 0)
		y2 = y;
}

// Parses <Main><Menu>.
static void ParseMenu(CSkin *skin, CXmlElement *menu)
{
	std::string value;

	for (std::vector<CXmlAttribute *>::iterator it = menu->m_attributes.begin(); it != menu->m_attributes.end(); ++it)
	{
		CXmlAttribute *attr = *it;
		if (attr == NULL)
			continue;
		value = attr->m_strValue;
		const std::string &name = attr->m_strName;
		if (StrCmpNoCase(name, "Smooth") == 0)
			skin->m_bSmooth = !IsFalseString(value.c_str());
		else if (StrCmpNoCase(name, "ShowIcon") == 0)
			skin->m_bShowIcon = !IsFalseString(value.c_str());
		else if (StrCmpNoCase(name, "ShowArrows") == 0)
			skin->m_bShowArrows = !IsFalseString(value.c_str());
		else if (StrCmpNoCase(name, "Rotate") == 0)
			skin->m_bRotate = IsTrueString(value.c_str());
		else if (StrCmpNoCase(name, "Scroll") == 0)
			skin->m_bScroll = IsTrueString(value.c_str()) || value.compare("2") == 0;
		else if (StrCmpNoCase(name, "Align") == 0)
		{
			if (StrCmpNoCase(value.c_str(), "middle") == 0)
				skin->m_nAlign = 1;
			else
				skin->m_nAlign = StrCmpNoCase(value.c_str(), "Right") == 0 ? 2 : 0;
		}
		else if (StrCmpNoCase(name, "Horizontal") == 0)
			skin->m_bHorizontal = IsTrueString(value.c_str());
	}

	// <ItemPos Left Top Width Height Align/> slots, up to 19.
	CXmlElement *itemPos = menu->FindFirstChild("ItemPos");
	if (itemPos != NULL)
	{
		skin->m_nItemPosCount = 1;
		do
		{
			if (skin->m_nItemPosCount > SKIN_MAX_ITEMPOS)
				break;
			int top = 0, left = 0, width = 0, height = 0;
			DWORD align = 0;
			if (!itemPos->m_attributes.empty())
			{
				for (std::vector<CXmlAttribute *>::iterator it = itemPos->m_attributes.begin();
				     it != itemPos->m_attributes.end(); ++it)
				{
					CXmlAttribute *attr = *it;
					if (attr == NULL)
						continue;
					long v = AttrLong(attr);
					if (v == 0)
					{
						if (StrCmpNoCase(attr->m_strName, "Align") == 0)
							align = ParseSkinAlign(attr->m_strValue.c_str());
					}
					else if (StrCmpNoCase(attr->m_strName, "Top") == 0)
						top = v;
					else if (StrCmpNoCase(attr->m_strName, "Left") == 0)
						left = v;
					else if (StrCmpNoCase(attr->m_strName, "Width") == 0)
						width = v;
					else if (StrCmpNoCase(attr->m_strName, "Height") == 0)
						height = v;
				}
				// Note: vertical centering is applied here and again below.
				if (height != 0 && (align & SKIN_ALIGN_CENTER_Y))
					top += (unsigned)height >> 1;
				if (width != 0)
					align |= SKIN_ALIGN_TRUNCATED;
			}

			if (align & SKIN_ALIGN_RIGHT)
				left += width;
			else if (align & SKIN_ALIGN_CENTER_X)
				left += (unsigned)width >> 1;
			if (width != 0)
				align |= SKIN_ALIGN_TRUNCATED;
			if (height != 0 && (align & SKIN_ALIGN_CENTER_Y))
				top += (unsigned)height >> 1;

			SSkinItemPos &slot = skin->m_itemPos[skin->m_nItemPosCount - 1];
			slot.align = align;
			slot.x = left;
			slot.y = top;
			slot.width = width;
			slot.height = height;
			skin->m_nItemPosCount++;
			itemPos = menu->FindNextChild();
		} while (itemPos != NULL);

		skin->m_nItemPosCount--;
		if (skin->m_nItemPosCount != 0)
			skin->m_bHorizontal = FALSE;
	}

	// <Font>file.xpr|file.ttf</Font> (Height or Size for TrueType, default 20)
	CXmlElement *fontElement = menu->FindFirstChild("Font");
	if (fontElement != NULL && fontElement->m_strValue.size() != 0)
	{
		value = fontElement->GetValue();
		if (value.size() != 0)
		{
			skin->FindFont(value.c_str(), (CXBFont **)&skin->m_pMenuFont);
			if (skin->m_pMenuFont == NULL)
			{
				skin->m_pMenuFont = new CXBFontEx();
				if (skin->m_pMenuFont != NULL)
				{
					value = skin->ResolveFile(value.c_str());
					wchar_t wideName[0x104];
					int size = 0;
					BOOL isTTF = StrCmpNoCase(StrRight(value, 3), "ttf") == 0;
					if (isTTF)
					{
						MultiByteToWideChar(CP_ACP, 0, value.c_str(), (int)value.size() + 1, wideName, 0x104);
						size = atol(fontElement->GetAttribute("Height").c_str());
						if (size == 0)
						{
							size = atol(fontElement->GetAttribute("Size").c_str());
							if (size == 0)
								size = 20;
						}
					}
					BOOL ok = StrCmpNoCase(StrRight(value, 3), "xpr") == 0 &&
					          SUCCEEDED(skin->m_pMenuFont->Create(value.c_str(), 0));
					if (!ok)
						ok = StrCmpNoCase(StrRight(value, 3), "ttf") == 0 &&
						     SUCCEEDED(skin->m_pMenuFont->CreateFromTTF(wideName, -1, size, 2));
					if (!ok)
					{
						skin->m_bOwnMenuFont = FALSE;
						delete skin->m_pMenuFont;
						skin->m_pMenuFont = NULL;
					}
					else
					{
						skin->m_bOwnMenuFont = TRUE;
					}
				}
			}
			else
			{
				skin->m_bOwnMenuFont = FALSE;
			}
		}
	}

	CXmlElement *child = menu->FindFirstChild("BackColor");
	if (child != NULL)
	{
		DWORD color = ParseHex(child->m_strValue);
		if (color != 0)
			skin->m_dwBackColor = color;
	}
	child = menu->FindFirstChild("BorderColor");
	if (child != NULL)
	{
		DWORD color = ParseHex(child->m_strValue);
		if (color != 0)
			skin->m_dwBorderColor = color;
		char *end;
		unsigned long w = strtoul(child->GetAttribute("Width").c_str(), &end, 10);
		if (w > 1 && w < 10)
			skin->m_nBorderWidth = w;
	}

	// Fixed/FixAt/MaxItem on <Menu>, or Fixed/FixAt on <Position> (which
	// also gives the menu area; heights under 50 become 270).
	CXmlElement *position = menu->FindFirstChild("Position");
	if (position == NULL)
	{
		value = menu->GetAttribute("Fixed");
		if (value.size() != 0 && IsTrueString(value.c_str()))
			skin->m_bFixed = TRUE;
		if (skin->m_bFixed)
		{
			DWORD fixAt = ParseHex(menu->GetAttribute("FixAt"));
			if (fixAt != 0)
				skin->m_nFixAt = fixAt;
		}
		DWORD maxItem = ParseHex(menu->GetAttribute("MaxItem"));
		if (maxItem != 0)
			skin->m_nMaxItem = maxItem;
	}
	else
	{
		ParseSkinRect(&skin->m_rcMenu, position);
		if (skin->m_rcMenu.width < 50)
			skin->m_rcMenu.width = 270;   // TODO: verify (original checks +0x30C)
		value = position->GetAttribute("Fixed");
		if (value.size() != 0 && IsTrueString(value.c_str()))
			skin->m_bFixed = TRUE;
		DWORD fixAt = 0;
		if (skin->m_bFixed)
		{
			fixAt = ParseHex(position->GetAttribute("FixAt"));
		}
		else
		{
			value = menu->GetAttribute("Fixed");
			if (value.size() != 0 && IsTrueString(value.c_str()))
				skin->m_bFixed = TRUE;
			if (skin->m_bFixed)
				fixAt = ParseHex(menu->GetAttribute("FixAt"));
		}
		if (fixAt != 0)
			skin->m_nFixAt = fixAt;
	}

	// <Preview Left Top Width Height EnableAudio EnableMusic/>: game preview video area.
	CXmlElement *preview = menu->FindFirstChild("Preview");
	if (preview != NULL)
	{
		ParseSkinRect(&skin->m_rcPreview, preview);
		skin->m_bPreview = skin->m_rcPreview.height != 0 && skin->m_rcPreview.width != 0;
		if (skin->m_bPreview)
		{
			if (skin->m_pPreviewVideo == NULL)
				skin->m_pPreviewVideo = new CWMVideoPlayer();
			if (skin->m_pPreviewVideo != NULL)
			{
				skin->m_pPreviewVideo->SetFont(skin->m_pInfoFont);
				skin->m_pPreviewVideo->SetFullScreen(FALSE);
				skin->m_pPreviewVideo->m_bModal = FALSE;
				skin->m_bPreviewAudio = !IsFalseString(preview->GetAttribute("EnableAudio").c_str());
				skin->m_bPreviewMusic = IsTrueString(preview->GetAttribute("EnableMusic").c_str());
				skin->m_pPreviewVideo->SetAudio(skin->m_bPreviewAudio != 0);
				skin->m_pPreviewVideo->SetRect(skin->m_rcPreview.top, skin->m_rcPreview.left,
				                               skin->m_rcPreview.width, skin->m_rcPreview.height);
			}
		}
	}
}

// Parses <Main> (except the <Text>/<Shape>/<Image> lists).
static void ParseMain(CSkin *skin, CXmlElement *main)
{
	std::string value;

	value = main->GetAttribute("Globe");
	if (value.size() != 0 && IsFalseString(value.c_str()))
	{
		skin->m_bGlobe = FALSE;
	}
	else
	{
		value = main->GetAttribute("Fog");
		if (value.size() != 0 && IsFalseString(value.c_str()))
			skin->m_bFog = FALSE;
	}
	value = main->GetAttribute("Orb");
	if (value.size() != 0 && IsFalseString(value.c_str()))
		skin->m_bOrb = FALSE;

	// <BackGround EnableAudio=...>image or .wmv/.xmv</BackGround>
	CXmlElement *background = main->FindFirstChild("BackGround");
	if (GetSkinFile(skin, background, value))
	{
		if (!EndsWithVideo(value))
		{
			if (!LoadTextureFromFile(value.c_str(), g_pd3dDevice, &skin->m_pBackground, 1, 1, NULL))
				skin->m_pBackground = NULL;
		}
		else
		{
			skin->m_pBackgroundVideo = new CWMVideoPlayer();
			if (skin->m_pBackgroundVideo != NULL)
			{
				skin->m_pBackgroundVideo->SetFont(skin->m_pInfoFont);
				skin->m_pBackgroundVideo->SetFullScreen(TRUE);
				skin->m_pBackgroundVideo->m_bModal = FALSE;
				skin->m_pBackgroundVideo->SetAudio(FALSE);
				std::string audio = background->GetAttribute("EnableAudio");
				if (audio.size() != 0 && IsTrueString(audio.c_str()))
					skin->m_pBackgroundVideo->SetAudio(TRUE);
				if (!skin->m_pBackgroundVideo->Open(value.c_str()))
				{
					delete skin->m_pBackgroundVideo;
					skin->m_pBackgroundVideo = NULL;
				}
			}
		}
	}

	if (GetSkinFile(skin, main->FindFirstChild("Orb"), value))
	{
		if (!LoadTextureFromFile(value.c_str(), g_pd3dDevice, &skin->m_pOrbTexture, 1, 1, NULL))
			skin->m_pOrbTexture = NULL;
	}
	if (GetSkinFile(skin, main->FindFirstChild("Cellwall"), value))
	{
		if (!LoadTextureFromFile(value.c_str(), g_pd3dDevice, &skin->m_pCellwall, 1, 1, NULL))
			skin->m_pCellwall = NULL;
	}

	// <GameIcon Enable Opacity><Position .../></GameIcon>
	CXmlElement *gameIcon = main->FindFirstChild("GameIcon");
	if (gameIcon != NULL)
	{
		g_bSkinGameIcon = IsTrueString(gameIcon->GetAttribute("Enable").c_str());
		if (g_bSkinGameIcon)
		{
			char *end;
			unsigned long opacity = strtoul(gameIcon->GetAttribute("Opacity").c_str(), &end, 10);
			if (opacity != 0)
				skin->m_fGameIconOpacity = (opacity >= 100 ? 100.0f : (float)opacity) * kPercent;
			ParseSkinRect(&skin->m_rcGameIcon, gameIcon->FindFirstChild("Position"));
		}
	}

	CXmlElement *menu = main->FindFirstChild("Menu");
	if (menu != NULL)
		ParseMenu(skin, menu);
}

// 0x000FD644
BOOL CSkin::Parse(CXmlDocument *doc)
{
	std::string value;
	CXmlElement *element;

	Free();
	SetDefaults();

	// <Skin Name="..."><Information><Author/><Version/></Information>
	element = doc->FindElement(NULL, "Information");
	doc->m_pCurrent = &doc->m_root;
	value = doc->m_root.GetAttribute("Name");
	MultiByteToWideChar(CP_ACP, 0, value.c_str(), (int)value.size() + 1, m_szName, 0x50);
	if (element != NULL)
	{
		CXmlElement *child = element->FindFirstChild("Author");
		if (child != NULL)
		{
			value = child->GetValue();
			MultiByteToWideChar(CP_ACP, 0, value.c_str(), (int)value.size() + 1, m_szAuthor, 0x50);
		}
		child = element->FindFirstChild("Version");
		if (child != NULL)
		{
			value = child->GetValue();
			MultiByteToWideChar(CP_ACP, 0, value.c_str(), (int)value.size() + 1, m_szVersion, 0x50);
		}
	}

	// <ScreenSize Width Height/> picks the render size the skin was made for.
	element = doc->FindElement(NULL, "ScreenSize");
	if (element != NULL)
	{
		unsigned int w = (unsigned int)atol(element->GetAttribute("Width").c_str());
		unsigned int h = (unsigned int)atol(element->GetAttribute("Height").c_str());
		int outW, outH = 720;
		bool pickHeight = true;
		if (w < 720)
			outW = 640;
		else if (w < 1280)
			outW = 720;
		else if (w >= 1920)
		{
			outW = 1920;
			h = 1080;
		}
		else
		{
			outW = 1280;
			pickHeight = false;   // 1280 wide always means 720 high
		}
		if (pickHeight)
		{
			if (h < 576)
				outH = 480;
			else if (h < 720)
				outH = m_pSettings->m_video.m_bPAL ? 576 : 480;
			else if (h > 1079)
				outH = 1080;
		}
		m_pSettings->m_nSkinHeight = outH;
		m_pSettings->m_nSkinWidth = outW;
	}

	// <Fonts><Font Name="...">file.xpr</Font></Fonts>
	element = doc->FindElement(NULL, "Fonts");
	if (element != NULL)
	{
		m_pFonts = new CFontTable();
		if (m_pFonts != NULL)
		{
			std::string name;
			for (CXmlElement *font = element->FindFirstChild("Font"); font != NULL; font = element->FindNextChild())
			{
				name = font->GetAttribute("Name");
				value = font->GetValue();
				value = StrTrimRight(StrTrimLeft(value));
				if (name.size() == 0 || value.size() == 0)
					continue;
				value = ResolveFile(value.c_str());
				CXBFontEx *xbfont = new CXBFontEx();
				if (xbfont == NULL)
					continue;
				if (SUCCEEDED(xbfont->Create(value.c_str(), 0)) && SUCCEEDED(m_pFonts->Add(name.c_str(), xbfont, TRUE)))
					continue;
				delete xbfont;
			}
		}
	}

	element = doc->FindElement(NULL, "Main");
	if (element == NULL)
	{
		SetDefaultMainLayout();
	}
	else
	{
		ParseMain(this, element);

		element = doc->FindElement(NULL, "Main");
		if (element != NULL)
		{
			// <InfoFont>file.xpr</InfoFont>
			CXmlElement *infoFont = element->FindFirstChild("InfoFont");
			if (infoFont != NULL && infoFont->m_strValue.size() != 0)
			{
				value = infoFont->GetValue();
				FindFont(value.c_str(), &m_pInfoFont);
				if (m_pInfoFont == NULL)
				{
					m_pInfoFont = new CXBFont();
					if (m_pInfoFont != NULL)
					{
						value = ResolveFile(value.c_str());
						if (FAILED(m_pInfoFont->Create(value.c_str(), 0)))
						{
							m_bOwnInfoFont = FALSE;
							delete m_pInfoFont;
							m_pInfoFont = NULL;
						}
						else
						{
							m_bOwnInfoFont = TRUE;
						}
					}
				}
				else
				{
					m_bOwnInfoFont = FALSE;
				}
			}
			ParseTexts(element, "Text", m_mainTexts, &m_mainOrder);
		}
		ParseShapes(element, "Shape", m_mainShapes, &m_mainOrder);
		ParseImages(element, "Image", m_mainImages, &m_mainOrder);
		if (m_bSortByZIndex)
			std::sort(m_mainOrder.begin(), m_mainOrder.end(), CompareSkinZIndex);   // 0x000274D4 TODO: verify ordering of ties

		m_bColorScheme = doc->FindElementRecursive("ColorScheme", NULL) != NULL;
		if (m_bColorScheme)
			LoadSkinColorScheme(doc);
	}

	// <FileManager SafeBorder DrawBorder><BackGround>image</BackGround></FileManager>
	element = doc->FindElement(NULL, "FileManager");
	if (element != NULL)
	{
		if (GetSkinFile(this, element->FindFirstChild("BackGround"), m_strFileManagerBackground))
			StrReplaceAll(m_strFileManagerBackground, "E:\\UDATA\\9e115330\\", "U:\\");
		if (IsFalseString(element->GetAttribute("SafeBorder").c_str()))
			m_bFileManagerSafeBorder = FALSE;
		m_bFileManagerDrawBorder = IsTrueString(element->GetAttribute("DrawBorder").c_str());
	}

	// <LoadingScreen ShowInfo><BackGround>image|video|GamePreview|GameIcon</BackGround></LoadingScreen>
	element = doc->FindElement(NULL, "LoadingScreen");
	if (element != NULL)
	{
		value = element->GetAttribute("ShowInfo");
		if (value.size() != 0 && IsFalseString(value.c_str()))
			m_bLoadingShowInfo = FALSE;

		CXmlElement *background = element->FindFirstChild("BackGround");
		if (background != NULL && background->m_strValue.size() != 0)
		{
			value = background->GetValue();
			if (StrCmpNoCase(value.c_str(), "GamePreview") == 0)
			{
				m_strLoadingPreview = ResolveFile(value.c_str());
			}
			else if (StrCmpNoCase(value.c_str(), "GameIcon") == 0)
			{
				m_strLoadingBackground = ResolveFile(value.c_str());
			}
			else
			{
				value = ResolveFile(value.c_str());
				std::string &target = EndsWithVideo(value) ? m_strLoadingPreview : m_strLoadingBackground;
				target = ResolveFile(value.c_str());
				StrReplaceAll(target, "E:\\UDATA\\9e115330\\", "U:\\");
			}
		}
	}

	// <IntroMovie>file.wmv|file.xmv</IntroMovie>
	element = doc->FindElement(NULL, "IntroMovie");
	if (element != NULL)
	{
		m_strIntroMovie = element->GetValue();
		if (m_strIntroMovie.size() != 0)
		{
			if (!EndsWithVideo(m_strIntroMovie))
				m_strIntroMovie.clear();
			else
				m_strIntroMovie = ResolveFile(m_strIntroMovie.c_str());
		}
	}

	if (m_pInfoFont == NULL)
		g_fonts.Find("InfoFont", &m_pInfoFont);
	if (m_pMenuFont == NULL)
		g_fonts.Find("MenuFont", (CXBFont **)&m_pMenuFont);
	if (m_pMenuFont != NULL)
		m_pMenuFont->SetScaleFactors(m_fMenuFontScaleX, m_fMenuFontScaleY);

	// A video background replaces the globe, orb and fog.
	if (m_pBackgroundVideo != NULL)
	{
		m_pBackgroundVideo->SetFont(m_pInfoFont);
		m_bGlobe = FALSE;
		m_bOrb = FALSE;
		m_bFog = FALSE;
	}
	if (m_pPreviewVideo != NULL)
		m_pPreviewVideo->SetFont(m_pInfoFont);

	// <MusicPlayer Left Top Width Height Relative>...</MusicPlayer>
	element = doc->FindElement(NULL, "MusicPlayer");
	if (element == NULL)
	{
		SetDefaultMusicPlayerLayout();
	}
	else
	{
		for (std::vector<CXmlAttribute *>::iterator it = element->m_attributes.begin();
		     it != element->m_attributes.end(); ++it)
		{
			CXmlAttribute *attr = *it;
			if (attr == NULL)
				continue;
			long v = AttrLong(attr);
			if (v == 0)
			{
				if (StrCmpNoCase(attr->m_strName, "Relative") == 0)
					m_bMusicPlayerRelative = IsTrueString(attr->m_strValue.c_str());
			}
			else if (StrCmpNoCase(attr->m_strName, "Top") == 0)
				m_rcMusicPlayer.top = v;
			else if (StrCmpNoCase(attr->m_strName, "Left") == 0)
				m_rcMusicPlayer.left = v;
			else if (StrCmpNoCase(attr->m_strName, "Width") == 0)
				m_rcMusicPlayer.width = v;
			else if (StrCmpNoCase(attr->m_strName, "Height") == 0)
				m_rcMusicPlayer.height = v;
			else if (StrCmpNoCase(attr->m_strName, "Relative") == 0)
				m_bMusicPlayerRelative = TRUE;
		}

		if (GetSkinFile(this, element->FindFirstChild("BackGround"), value))
		{
			if (!LoadTextureFromFile(value.c_str(), g_pd3dDevice, &m_pMusicPlayerBackground, 1, 1, NULL))
				m_pMusicPlayerBackground = NULL;
		}
		if (GetSkinFile(this, element->FindFirstChild("ButtonMap"), value))
		{
			if (!LoadTextureFromFile(value.c_str(), g_pd3dDevice, &m_pButtonMap, 1, 1, NULL))
				m_pButtonMap = NULL;
		}

		ParseTexts(element, "Text", m_musicTexts, &m_musicOrder);
		ParseShapes(element, "Element", m_musicShapes, &m_musicOrder);
		ParseImages(element, "Image", m_musicImages, &m_musicOrder);
		std::sort(m_musicOrder.begin(), m_musicOrder.end(), CompareSkinZIndex);   // TODO: verify (always sorted here)

		if (m_bMusicPlayerRelative)
		{
			size_t i;
			for (i = 0; i < m_musicTexts.size(); i++)
			{
				m_musicTexts[i].left += m_rcMusicPlayer.left;
				m_musicTexts[i].top += m_rcMusicPlayer.top;
			}
			for (i = 0; i < m_musicShapes.size(); i++)
			{
				m_musicShapes[i].left += m_rcMusicPlayer.left;
				m_musicShapes[i].top += m_rcMusicPlayer.top;
			}
			for (i = 0; i < m_musicImages.size(); i++)
			{
				m_musicImages[i].left += m_rcMusicPlayer.left;
				m_musicImages[i].top += m_rcMusicPlayer.top;
			}
		}

		// <PlayList Left Top Width Height OffsetX OffsetY OffsetX2 OffsetY2><BackGround/></PlayList>
		CXmlElement *playList = element->FindFirstChild("PlayList");
		if (playList != NULL)
		{
			if (GetSkinFile(this, playList->FindFirstChild("BackGround"), value))
			{
				if (!LoadTextureFromFile(value.c_str(), g_pd3dDevice, &m_pPlayListBackground, 1, 1, NULL))
					m_pPlayListBackground = NULL;
			}
			if (m_pPlayListBackground != NULL)
			{
				ParseSkinRect(&m_rcPlayList, playList);
				int x2 = 0, y2 = 0;
				ParseOffsets(playList, m_nPlayListOffsetX, m_nPlayListOffsetY, x2, y2);
				m_nPlayListOffsetX2 = x2;
				m_nPlayListOffsetY2 = y2;
			}
			if (m_nPlayListOffsetX2 == 0)
				m_nPlayListOffsetX2 = m_nPlayListOffsetX;
			if (m_nPlayListOffsetY2 == 0)
				m_nPlayListOffsetY2 = m_nPlayListOffsetY;
		}

		// <Dialog OffsetX OffsetY OffsetX2 OffsetY2><BackGround/></Dialog>
		CXmlElement *dialog = element->FindFirstChild("Dialog");
		if (dialog != NULL)
		{
			if (GetSkinFile(this, dialog->FindFirstChild("BackGround"), value))
			{
				if (!LoadTextureFromFile(value.c_str(), g_pd3dDevice, &m_pDialogBackground, 1, 1, NULL))
					m_pDialogBackground = NULL;
			}
			if (m_pDialogBackground != NULL)
			{
				int x2 = 0, y2 = 0;
				ParseOffsets(dialog, m_nDialogOffsetX, m_nDialogOffsetY, x2, y2);
				m_nDialogOffsetX2 = x2;
				m_nDialogOffsetY2 = y2;
			}
			if (m_nDialogOffsetX2 == 0)
				m_nDialogOffsetX2 = m_nDialogOffsetX;
			if (m_nDialogOffsetY2 == 0)
				m_nDialogOffsetY2 = m_nDialogOffsetY;
		}
	}

	if (m_bSortByZIndex)
		m_bSortByZIndex = !m_mainOrder.empty();

	BOOL unused = FALSE;
	PollLoad(&unused);   // 0x000F8E2B
	return TRUE;
}
