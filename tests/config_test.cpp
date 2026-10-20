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

	// Volumes, the screen adjustment and the 5.1 mix bins.
	int failures = 0;
	SAudioSettings saved = s->m_audio;
	const struct { int volume; BOOL effects; LONG sound, music; } volumes[] = {
		{100, TRUE, 0, 0}, {99, TRUE, 0, 0}, {50, TRUE, -2500, -2500}, {4, TRUE, -4800, -4800},
		{3, TRUE, -6400, -6400}, {50, FALSE, -6400, -2500},
	};
	for (size_t i = 0; i < sizeof(volumes) / sizeof(volumes[0]); i++)
	{
		s->m_audio.m_nSoundVolume = s->m_audio.m_nMusicVolume = volumes[i].volume;
		s->m_audio.m_bSoundEffects = volumes[i].effects;
		if (s->GetSoundVolume() != volumes[i].sound || s->GetMusicVolume() != volumes[i].music)
		{
			printf("FAIL volume %d: %ld %ld\n", volumes[i].volume, (long)s->GetSoundVolume(), (long)s->GetMusicVolume());
			failures++;
		}
	}
	if (s->GetScreenAdjust() != &s->m_video.m_screenAdjust[SCREEN_640x480])
		printf("FAIL GetScreenAdjust\n"), failures++;
	s->m_audio.m_bEnable51 = FALSE;
	s->m_audio.SetEnable51(TRUE);
	if (s->m_audio.m_speakerMixBins.dwMixBinCount != 6 || s->m_audio.m_speakerMixBins.pairs[4].dwMixBin != 2 ||
	    s->m_audio.m_speakerMixBins.lpMixBinVolumePairs != s->m_audio.m_speakerMixBins.pairs)
		printf("FAIL SetEnable51(TRUE)\n"), failures++;
	s->m_audio.SetEnable51(FALSE);
	if (s->m_audio.m_speakerMixBins.dwMixBinCount != 2 || s->m_audio.m_speakerMixBins.pairs[1].dwMixBin != 1)
		printf("FAIL SetEnable51(FALSE)\n"), failures++;
	s->m_audio = saved;
	return failures != 0;
}
