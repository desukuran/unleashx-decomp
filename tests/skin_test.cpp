// Host test: parse a Skin.xml with the reconstructed CSkin::Parse and dump it.
//   skin_test <dir containing Skin.xml>
#include <cstdio>
#include <string>

#include "../src/settings/AppSettings.h"
#include "../src/skin/Skin.h"
#include "../src/xml/XmlDocument.h"

int main(int argc, char **argv)
{
	if (argc < 2)
		return 1;
	CAppSettings *settings = CAppSettings::GetInstance();
	g_pSettings = settings;
	settings->m_strHomePath = "e:\\dash\\";

	std::string dir = argv[1];
	CXmlDocument doc;
	if (dir.size() < 4 || dir.compare(dir.size() - 4, 4, ".xml") != 0)
		dir += "/Skin.xml";
	if (!doc.Load(dir.c_str()))
	{
		fprintf(stderr, "load failed\n");
		return 1;
	}
	g_skin.m_pSettings = settings;
	g_skin.m_strSkinDir = "e:\\dash\\skins\\test\\";
	g_skin.Parse(&doc);

	printf("name=%ls globe=%d orb=%d fog=%d\n", g_skin.m_szName, g_skin.m_bGlobe, g_skin.m_bOrb, g_skin.m_bFog);
	printf("menu rect %d,%d %dx%d smooth=%d align=%d showicon=%d rotate=%d scroll=%d itempos=%d\n",
	       g_skin.m_rcMenu.left, g_skin.m_rcMenu.top, g_skin.m_rcMenu.width, g_skin.m_rcMenu.height,
	       g_skin.m_bSmooth, g_skin.m_nAlign, g_skin.m_bShowIcon, g_skin.m_bRotate, g_skin.m_bScroll,
	       g_skin.m_nItemPosCount);
	for (int i = 0; i < g_skin.m_nItemPosCount; i++)
		printf("  itempos %d: %d,%d %dx%d align=%u\n", i, g_skin.m_itemPos[i].x, g_skin.m_itemPos[i].y,
		       g_skin.m_itemPos[i].width, g_skin.m_itemPos[i].height, (unsigned)g_skin.m_itemPos[i].align);
	printf("preview=%d %d,%d %dx%d backcolor=%08x\n", g_skin.m_bPreview, g_skin.m_rcPreview.left,
	       g_skin.m_rcPreview.top, g_skin.m_rcPreview.width, g_skin.m_rcPreview.height,
	       (unsigned)g_skin.m_dwBackColor);
	for (size_t i = 0; i < g_skin.m_mainTexts.size(); i++)
	{
		const SSkinText &t = g_skin.m_mainTexts[i];
		printf("  text src=%#x at %d,%d %dx%d color=%08x align=%#x fmt=%d text=%ls\n", t.source, t.left, t.top,
		       t.width, t.height, (unsigned)t.color, (unsigned)t.align, t.isFormat, t.text ? t.text : L"");
	}
	printf("shapes=%zu images=%zu music: texts=%zu shapes=%zu images=%zu\n", g_skin.m_mainShapes.size(),
	       g_skin.m_mainImages.size(), g_skin.m_musicTexts.size(), g_skin.m_musicShapes.size(),
	       g_skin.m_musicImages.size());
	return 0;
}
