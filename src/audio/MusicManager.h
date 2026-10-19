// Background music (CMusicManager): plays soundtracks. The soundtrack list
// is the audio CD (index 0), "Media Player" (1: a file or folder chosen in
// the file manager), then the Xbox's own soundtracks and the folders with
// MP3/WMA files under <Audio MusicPath> (2 and up). A thread keeps the
// current player fed; the main loop calls Update to move on to the next
// song. Folder titles and song lengths/titles are cached in Info.xml in the
// music folder, keyed by the MD5 of the folder path.
#pragma once

#include <string>
#include <vector>

#include "../common/Thread.h"
#include "../system/SystemInfo.h"
#include "AudioPlayer.h"

class CAppSettings;
class CXmlDocument;
class CXmlElement;
struct IDirect3DTexture8;
struct SGamepadState;

enum MusicState
{
	MUSIC_STOPPED = 0,
	MUSIC_PAUSED = 1,
	MUSIC_PLAYING = 2,
};

// The fixed soundtracks.
#define SOUNDTRACK_CD 0
#define SOUNDTRACK_MEDIA_PLAYER 1
#define SOUNDTRACK_FIRST_USER 2

// CMusicManager::m_nSong when no song is selected.
#define MUSIC_NO_SONG 100000

// A soundtrack. 0x164 bytes.
struct SSoundtrack
{
	BYTE unknown00[0x10];       // +0x000
	wchar_t name[32];           // +0x010
	UINT songCount;             // +0x050
	char path[0x104];           // +0x054 a folder ("cdrom:" for the CD)
	BOOL isFolder;              // +0x158 a music folder, not an Xbox soundtrack
	DWORD soundtrackId;         // +0x15C XDK soundtrack ID
	UINT index;                 // +0x160 its position in the list
};

// A song. 0x184 bytes.
struct SSong
{
	wchar_t name[33];           // +0x000
	char path[0x132];           // +0x042 file (or the track number for a CD)
	DWORD length;               // +0x174 ms; 999000 until known
	UINT index;                 // +0x178 in its soundtrack
	int type;                   // +0x17C AudioType
	DWORD songId;               // +0x180 XDK song ID
};

#define SONG_LENGTH_UNKNOWN 999000

// The shuffled soundtrack order (global random play). 8 bytes.
struct SSoundtrackOrder
{
	UINT soundtrack;            // +0x00
	UINT unused;                // +0x04
};

// The music event interface (RTTI CMusicEvent). TODO: what slots 0 and 1
// are for.
class CMusicEvent
{
public:
	virtual BOOL Unknown0() { return TRUE; }                 // slot 0, 0x000111E7
	virtual int Unknown1() { return 0; }                     // slot 1
	virtual HRESULT Init(const char *dataPath) = 0;          // slot 2
};

// 0x23C bytes.
class CMusicManager : public CMusicEvent, public Thread
{
public:
	// useSoundtracks: the Xbox soundtracks; useFolder: the MP3/WMA folders
	// under folder (if it exists). (this in esi)
	CMusicManager(BOOL useSoundtracks, BOOL useFolder, const char *folder);   // 0x00106D35
	~CMusicManager();                                        // 0x00106283 (this in edi)

	virtual int Unknown1() { return 0; }                     // slot 1, 0x00106110
	// Finds the soundtracks, creates the players and loads a random
	// soundtrack (not playing yet). (this in ecx)
	virtual HRESULT Init(const char *dataPath);              // slot 2, 0x00108CF2
	// The thread: starts the player and keeps feeding it.
	virtual unsigned Run();                                  // Thread slot 0, 0x00108297

	int GetState() const { return m_nState; }                // 0x00105FCE (this in eax) MusicState
	// The playing stream's format, NULL when stopped. (this in eax)
	SSongFormat *GetSongFormat();                            // 0x00105FD5
	float GetSongPosition();                                 // 0x00106017 (this in eax) seconds
	// The soundtracks after the CD and Media Player. (this in eax)
	UINT GetUserSoundtrackCount();                           // 0x00105FEC
	UINT GetSongCount();                                     // 0x0010619B (this in eax)
	// The current soundtrack's name (31 characters). (this in eax)
	void GetSoundtrackName(wchar_t *name);                   // 0x00106189
	// The current soundtrack's folder.jpg/png, loaded on first use.
	// (this in eax)
	IDirect3DTexture8 *GetAlbumArt();                        // 0x00106225
	// Names of the soundtrack and song and the song length (ms); any can be
	// NULL. Quirk: until the length is known the call only asks the player
	// (and updates Info.xml) without filling them in. (this in eax)
	HRESULT GetSongInfo(wchar_t *soundtrack, wchar_t *song, DWORD *length);   // 0x00106FA9
	// Renames a music-folder soundtrack (saved in Info.xml). (this in edi,
	// index in ecx)
	BOOL SetSoundtrackTitle(UINT index, const char *title);  // 0x00106EF1

	BOOL IsPlaying();                                        // 0x00106173 (this in ecx)
	BOOL IsRandom() const { return m_bRandom; }              // 0x00106010 (this in eax)
	BOOL IsGlobal() const { return m_bGlobal; }              // 0x001060C8 (this in eax)
	void SetRandom(BOOL random) { m_bRandom = random; }      // 0x001060BF (this in eax, random in ecx)
	void SetGlobal(BOOL global) { m_bGlobal = global; }      // 0x001060B6 (this in eax, global in ecx)
	float GetVolume();                                       // 0x00105FB7 (this in eax)
	HRESULT SetVolume(float volume);                         // 0x0010607A (this in eax) hundredths of a dB
	void SetMute(BOOL mute);                                 // 0x0010615A (this in eax, mute in ecx)
	// NULL = stereo. Restarts playback if it was playing.
	void SetSpeakerConfig(const void *config);               // 0x001087DF

	HRESULT Play();                                          // 0x001083B0 (this in edi) start/resume
	HRESULT Pause();                                         // 0x0010602C (this in esi) 1 if not playing
	// Plays again if paused (not stopped, not paused by the user).
	void Resume();                                           // 0x00108E69 (this in eax)
	HRESULT Stop();                                          // 0x001061A5 (this in eax)
	HRESULT PlayAudioCD();                                   // 0x001084E7 (this in eax)
	// Plays one file, or (folder) the songs under a folder, as the Media
	// Player soundtrack. (this in eax)
	BOOL PlayFile(const char *path, BOOL folder);            // 0x00108523
	// Loads soundtrack index and starts its first song. reshuffle: draw a
	// new global order. (this on the stack)
	HRESULT LoadSoundtrack(UINT index, BOOL reshuffle);      // 0x001075DD
	// The Xbox soundtrack with this ID. (this on the stack)
	HRESULT SelectSoundtrack(DWORD soundtrackId);            // 0x00107FDA
	HRESULT NextSong();                                      // 0x0010756E (this in eax)
	HRESULT PrevSong();                                      // 0x00108855 (this in eax)
	HRESULT NextSoundtrack();                                // 0x001081AE (this in eax)
	HRESULT PrevSoundtrack();                                // 0x001088DC (this in eax)
	// Moves on when the song is over. Called by the main loop.
	HRESULT Update();                                        // 0x0010896C (this in eax)
	// The music keys (with music volume 5+): the right stick click toggles pause; with WHITE held the
	// d-pad changes song (left/right) and soundtrack (up/down); the
	// remote's transport keys work directly. (this in ecx, pad in eax)
	BOOL HandleInput(int *action, SGamepadState **pad);      // 0x00108E87

	// (public: the music player screen reads the lists)
	BOOL m_bReady;                         // +0x014 no song change in progress
	int m_unknown18;                       // +0x018
	BOOL m_bPlayingCD;                     // +0x01C the player is the CD player
	int m_nCdTracks;                       // +0x020
	BOOL m_bMuted;                         // +0x024
	CMD5 m_md5;                            // +0x028 of a music folder's path (m_szHex keys Info.xml)
	CAudioPlayer *m_pPlayer;               // +0x05C the current one
	CAudioPlayer *m_pPlayers[4];           // +0x060 WMA, MP3, CD, WAV
	UINT m_nPlayers;                       // +0x070
	CAppSettings *m_pSettings;             // +0x074
	HANDLE m_hWakeEvent;                   // +0x078 ends the thread's wait
	BOOL m_bSongActive;                    // +0x07C cleared when the player reports the end
	DWORD m_dwNextCheck;                   // +0x080 GetTickCount() before Update may move on
	BOOL m_bAdvance;                       // +0x084 Update may move on
	BOOL m_bGlobal;                        // +0x088 random across soundtracks
	BOOL m_bRandom;                        // +0x08C
	BOOL m_bShuffled;                      // +0x090 m_shuffled is in use
	float m_fVolume;                       // +0x094
	std::string m_strUnknown98;            // +0x098 "|"
	BOOL m_bNewSoundtrack;                 // +0x0B4 the next song is the first
	BOOL m_bLoadingSong;                   // +0x0B8
	int m_unknownBC;                       // +0x0BC
	int m_nState;                          // +0x0C0 MusicState
	wchar_t m_szSoundtrack[65];            // +0x0C4
	wchar_t m_szSong[33];                  // +0x146
	DWORD m_dwSongLength;                  // +0x188 ms, 0 until known
	int m_unknown18C;                      // +0x18C
	std::vector<SSoundtrack> m_soundtracks;   // +0x190
	UINT m_nSoundtrack;                    // +0x1A0
	UINT m_nSong;                          // +0x1A4 MUSIC_NO_SONG = none
	UINT m_nSongNumber;                    // +0x1A8 the song's index in its soundtrack
	UINT m_nOrderPos;                      // +0x1AC in m_order
	SSoundtrack *m_pSoundtrack;            // +0x1B0
	std::vector<SSong> m_songs;            // +0x1B4
	std::vector<SSong> m_shuffled;         // +0x1C4 the songs in random order
	BOOL m_bUseSoundtracks;                // +0x1D4
	BOOL m_bUseMusicFolder;                // +0x1D8
	std::string m_strMusicFolder;          // +0x1DC
	BOOL m_bSongEnded;                     // +0x1F8
	CXmlDocument *m_pInfo;                 // +0x1FC <music folder>\Info.xml
	CXmlElement *m_pFolderInfo;            // +0x200 the current folder's element
	std::vector<SSoundtrackOrder> m_order; // +0x204
	IDirect3DTexture8 *m_pAlbumArt;        // +0x214
	std::string m_strAlbumArt;             // +0x218
	BOOL m_bUserPaused;                    // +0x234
	BOOL m_bLastSong;                      // +0x238 the soundtrack wrapped around

private:
	void StopPlayer();                                       // 0x0010605F (this in esi)
	CAudioPlayer *FindPlayer(int type);                      // 0x001060CF (this in esi)
	static void SeedRandom();                                // 0x00106113
	// Moves songs into m_shuffled in random order (random play only).
	// (songs in edi, this on the stack)
	void ShuffleSongs(std::vector<SSong> &songs);            // 0x001063DD
	BOOL AddSoundtrack(const char *path, const wchar_t *name);   // 0x001064A1 (this in eax, name in ebx)
	// m_strAlbumArt: folder.jpg (else folder.png) next to path. (this on
	// the stack)
	void FindAlbumArt(const char *path);                     // 0x0010656E
	// The MP3/WMA files under path (recursively) into m_songs (and, with
	// random play, into shuffle). (this in ecx)
	void AddFolderSongs(const char *path, std::vector<SSong> *shuffle);   // 0x001065FC
	// Every folder under path with MP3/WMA files becomes a soundtrack.
	// (this in ecx)
	void ScanMusicFolder(const char *path, const wchar_t *name);   // 0x0010693B
	// (this in ebx)
	HRESULT PlaySong(UINT index, BOOL shuffled);             // 0x00107256
	HRESULT PlaySongAt(UINT index) { return PlaySong(index, FALSE); }   // 0x00107560 (this in ebx)
	HRESULT LoadSoundtracks();                               // 0x00108A75 (this on the stack)
};

extern CMusicManager *g_pMusicManager;  // 0x0027622C
