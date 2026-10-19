// Host test: run the reconstructed config loader on a Config.xml.
//   UNLEASHX_ROOT=<dir with E/dash/config.xml> config_test
#include <cstdio>

#include "../src/settings/AppSettings.h"
#include "../src/settings/ConfigLoader.h"
#include "../src/xml/XmlDocument.h"

#define S(x) printf("  %-28s = %s\n", #x, (x).c_str())
#define I(x) printf("  %-28s = %d\n", #x, (int)(x))

int main()
{
	CAppSettings *s = CAppSettings::GetInstance();
	g_pSettings = s;
	s->m_strHomePath = "e:\\dash\\";
	s->m_strXbeName = "default.xbe";
	s->m_bRunFromDVD = FALSE;

	ConfigLoaderThread(NULL);
	printf("loaded=%d\n", g_bConfigLoaded);

	S(s->m_strMSDashboard); S(s->m_strPassword); I(s->m_nPasswordMaxTries); I(s->m_bUnlocked);
	I(s->m_network.m_bEnable); I(s->m_network.m_bDHCP); S(s->m_network.m_strIP); S(s->m_network.m_strSubnet);
	S(s->m_network.m_strGateway); S(s->m_network.m_strDNS1); S(s->m_network.m_strDNS2);
	I(s->m_ftp.m_bEnable); I(s->m_ftp.m_nPort); I(s->m_ftp.m_nMaxUsers); I(s->m_ftp.m_bAllowAnon);
	S(s->m_ftp.m_strUser); S(s->m_ftp.m_strPassword); S(s->m_ftp.m_strGreeting); S(s->m_ftp.m_strSyst);
	I(s->m_bNewsfeedEnable); I(s->m_nNewsfeedInterval);
	I(s->m_prefs.m_bGamesAutoLaunch); I(s->m_prefs.m_bDVDAutoLaunch); S(s->m_prefs.m_strDVDPlayer);
	I(s->m_prefs.m_bAudioCDAutoLaunch); S(s->m_prefs.m_strAudioCDPlayer); I(s->m_prefs.m_bDataAutoLaunch);
	S(s->m_prefs.m_strDataLauncher); I(s->m_prefs.m_bSNTPSync); S(s->m_prefs.m_strSNTPServer);
	I(s->m_prefs.m_nSNTPPort); I(s->m_prefs.m_nFanSpeed); I(s->m_prefs.m_nScreenSaverWait);
	S(s->m_prefs.m_strScreenSaverText); I(s->m_prefs.m_nAutoTurnOff); I(s->m_prefs.m_bEnableDriveF);
	I(s->m_prefs.m_bEnableDriveG); S(s->m_prefs.m_strSkinPath); S(s->m_prefs.m_strSkinName);
	I(s->m_audio.m_nSoundVolume); S(s->m_audio.m_strMusicPath); S(s->m_audio.m_strSound[SOUND_SCROLL]);
	I(s->m_video.m_nFilterLevel); I(s->m_video.m_bSoften);

	CXmlElement *menu = g_config.m_root.FindFirstChild("Menu");
	printf("menu items: %d\n", menu != NULL ? menu->m_children.GetCount() : -1);
	return 0;
}
