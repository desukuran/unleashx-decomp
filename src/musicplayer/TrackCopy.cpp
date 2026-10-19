// CTrackCopy: copying songs into another (or a new) Xbox soundtrack, and
// the CD ripping it was meant to do. See TrackOptions.h.
#include "TrackOptions.h"

#include <cstdio>
#include <cstring>
#include <cwchar>

#include "../audio/AudioPlayer.h"
#include "../audio/SoundtrackDB.h"
#include "../common/StrUtil.h"
#include "../common/XboxKernel.h"
#include "../input/Gamepad.h"
#include "../render/Draw.h"
#include "../render/Fonts.h"
#include "../skin/SkinColors.h"
#include "../ui/Language.h"
#include "../ui/StringIds.h"
#include "../ui/VirtualKeyboard.h"

void DrawTrackButtons(float x, float y, float width, float height, float scaleY, CXBFont *font,
                      IDirect3DTexture8 *panel, const wchar_t *const *labels, UINT count, UINT selected);
void LayoutTrackScreen(CSongListControl *list, float *x, float *y, float *width, float *height, float *scaleX,
                       float *scaleY);
void InitTrackScreen(CSongListControl *list, CXBFont *font, IDirect3DTexture8 **panel);

#define TEMP_CDA_PATH "x:\\tempcda.cda"
#define TEMP_WMA_PATH "x:\\tempwma.wma"
#define COPY_BUFFER_SIZE 0x4000
#define CD_FRAMES_PER_SECOND 75
#define CD_SECTOR_SIZE 0x930

// 0x000F1FCF
CTrackCopy::CTrackCopy()
{
	m_dwSoundtrack = (DWORD)-1;
	m_fY = 0.0f;
	m_fX = 0.0f;
	m_fWidth = 0.0f;
	m_fHeight = 0.0f;
	m_fScaleY = 1.0f;
	m_fScaleX = 1.0f;
	m_pFont = NULL;
	m_bOpen = FALSE;
	m_pPanelTexture = NULL;
	m_nButton = 0;
	m_pMessageBox = NULL;
	m_bMessageShown = FALSE;
	m_bKeyboardOpen = FALSE;
	m_pTargetMenu = NULL;
	m_bTargetMenuOpen = FALSE;
	m_bNewSoundtrack = FALSE;
	m_strUnusedB4.clear();
	m_hThread = NULL;
	m_nProgress = 0;
	m_szSongName[0] = 0;
	m_nTrack = 0;
	m_nError = COPY_OK;
	m_pBuffer = NULL;
}

// 0x000F1AE1
CTrackCopy::~CTrackCopy()
{
	m_unusedD0.clear();
	if (m_pMessageBox != NULL)
	{
		delete m_pMessageBox;
		m_pMessageBox = NULL;
	}
	if (m_pTargetMenu != NULL)
	{
		delete m_pTargetMenu;
		m_pTargetMenu = NULL;
	}
	if (m_pBuffer != NULL)
		delete[] m_pBuffer;
	m_unusedD0.clear();
}

// 0x000F1F64
BOOL CTrackCopy::Init(CXBFontEx *font)
{
	m_pFont = (CXBFont *)font;
	InitTrackScreen(&m_songList, m_pFont, &m_pPanelTexture);
	Show();
	return S_OK;
}

// 0x000F1E22
void CTrackCopy::Show()
{
	m_bOpen = TRUE;
	LayoutTrackScreen(&m_songList, &m_fX, &m_fY, &m_fWidth, &m_fHeight, &m_fScaleX, &m_fScaleY);
}

// 0x000F11BA
void CTrackCopy::SetSoundtrack(DWORD soundtrack)
{
	m_dwSoundtrack = soundtrack;
	m_songList.SetSoundtrack((int)soundtrack);
}

// 0x000F313C
void CTrackCopy::ShowMessage(const char *text, const wchar_t *title, int buttons)
{
	ShowTrackMessage(&m_pMessageBox, &m_bMessageShown, m_pFont, text, title, buttons);
}

// 0x000F4899
BOOL CTrackCopy::Render()
{
	const wchar_t *labels[3] = {GetString(STR_SELECT), GetString(STR_SELECT_ALL), GetString(STR_COPY)};
	DrawTrackButtons(m_fX, m_fY, m_fWidth, m_fHeight, m_fScaleY, m_pFont, m_pPanelTexture, labels, 3, m_nButton);
	m_songList.Render();
	if (m_bKeyboardOpen)
		g_virtualKeyboard.Render();
	else if (m_bMessageShown)
		m_pMessageBox->Render();
	else if (m_bTargetMenuOpen)
		m_pTargetMenu->Render();
	if (m_hThread != NULL)
	{
		wchar_t text[256];
		swprintf(text, 256, L"Copying %s", m_szSongName);
		float lineHeight = m_pFont->m_fScaleFactorY * m_pFont->m_fFontHeight;
		DrawProgressBox(m_pFont, m_fX + 10.0f, m_fHeight * 0.5f + m_fY, m_fWidth,
		                lineHeight * 3.0f + m_fScaleY * 30.0f, L"Copying", text, NULL, (float)(DWORD)m_nProgress, 0.0f,
		                TRUE);
	}
	return S_OK;
}

// 0x000F3016
// "Copy": "New Soundtrack..." (value 0) and the other Xbox soundtracks
// (their IDs). Made once; a copy into a new soundtrack deletes it so it's
// made again with that one.
void CTrackCopy::ShowTargetMenu()
{
	if (m_pTargetMenu == NULL)
	{
		m_pTargetMenu = new COptionMenu;
		if (m_pTargetMenu != NULL)
		{
			m_pTargetMenu->Create(m_pFont, GetString(STR_COPY), 0.0f, 0.0f, 0.0f, 0.0f, 10);
			m_pTargetMenu->AddOption(GetString(STR_AUDIO_NEWTRACK), 0);
			XSOUNDTRACK_DATA data;
			HANDLE find = XFindFirstSoundtrack(&data);
			if (find != INVALID_HANDLE_VALUE)
			{
				do
				{
					if (m_dwSoundtrack != data.uSoundtrackId)
						m_pTargetMenu->AddOption(data.szName, (int)data.uSoundtrackId);
				} while (XFindNextSoundtrack(find, &data));
				XFindClose(find);
			}
		}
	}
	if (m_pTargetMenu == NULL)
	{
		m_bTargetMenuOpen = FALSE;
		return;
	}
	m_pTargetMenu->Show(0);
	m_bTargetMenuOpen = TRUE;
}

// 0x000F3953
// Quirk: B on the target menu starts a copy into soundtrack -1 (the menu's
// "no choice"), which then fails with the copy error.
HRESULT CTrackCopy::ProcessInput(int *action, SGamepadState **pad)
{
	if (m_hThread != NULL)
	{
		// A copy runs: no input.
		if (WaitForSingleObject(m_hThread, 50) == 0x102 /* WAIT_TIMEOUT */)
		{
			*action = INPUT_NONE;
			return S_OK;
		}
		CloseHandle(m_hThread);
		m_hThread = NULL;
		return S_OK;
	}

	if (m_bKeyboardOpen)
	{
		// The new soundtrack's name.
		g_virtualKeyboard.ProcessInput(action);
		m_bKeyboardOpen = !g_virtualKeyboard.IsDone();
		if (m_bKeyboardOpen || g_virtualKeyboard.m_bCancelled)
			return S_OK;
		wchar_t name[260];
		wcscpy(name, g_virtualKeyboard.GetText());
		if (!m_bNewSoundtrack || !OpenSoundtrackDB())
			return S_OK;
		DWORD target;
		if (!CreateSoundtrack(name, &target))
		{
			CloseSoundtrackDB();
			std::string text;
			StrFormat(text, GetStringA(STR_MSG_COPY_ERROR), GetLastError());
			ShowMessage(text.c_str(), GetString(STR_ERROR), MSGBOX_BUTTONS_OK);
			m_pTargetMenu->Show(0);
			m_pTargetMenu->m_bAnimate = FALSE;
			m_bNewSoundtrack = FALSE;
			return S_OK;
		}
		CloseSoundtrackDB();
		StartCopy(target);
		return S_OK;
	}

	if (m_bMessageShown)
	{
		m_pMessageBox->ProcessInput(action);
		m_bMessageShown = m_pMessageBox->IsOpen();
		if (m_bMessageShown || m_pMessageBox->GetResult() != MSGBOX_YES)
			return S_OK;
		// (The song editor's delete, left over: this screen never asks a
		// Yes/No question.)
		if (!OpenSoundtrackDB())
			return S_OK;
		if (DeleteSong(m_dwSoundtrack, m_songList.m_nSelected))
		{
			CloseSoundtrackDB();
			UINT selected = m_songList.m_nSelected;
			m_songList.SetSelection(selected != 0 ? selected - 1 : 0);
			m_songList.Reload();
			return S_OK;
		}
		std::string text = GetStringA(STR_MSG_DELETE_ERROR);
		StrFormat(text, text.c_str(), GetLastError());
		ShowMessage(text.c_str(), GetString(STR_ERROR), MSGBOX_BUTTONS_OK);
		CloseSoundtrackDB();
		return S_OK;
	}

	if (m_bTargetMenuOpen)
	{
		m_pTargetMenu->ProcessInput(action, pad);
		m_bTargetMenuOpen = m_pTargetMenu->IsOpen();
		if (m_bTargetMenuOpen)
			return S_OK;
		m_bTargetMenuOpen = FALSE;
		if (m_pTargetMenu->GetSelection() != 0)
		{
			StartCopy((DWORD)m_pTargetMenu->GetSelectedValue());
			return S_OK;
		}
		// A new soundtrack: the first free "Soundtrack <n>" as the
		// suggested name.
		for (UINT n = 1;; n++)
		{
			wchar_t name[32];
			swprintf(name, 32, L"Soundtrack %u", n);
			BOOL taken = FALSE;
			for (std::vector<COptionMenu::SOption>::iterator it = m_pTargetMenu->m_options.begin();
			     it != m_pTargetMenu->m_options.end(); ++it)
			{
				if (_wcsicmp(name, it->text.c_str()) == 0)
				{
					taken = TRUE;
					break;
				}
			}
			if (!taken)
			{
				OpenTrackKeyboard(GetString(STR_AUDIO_NEWTRACK), name, FALSE, 0x1F, 0, &m_bKeyboardOpen);
				m_bNewSoundtrack = TRUE;
				return S_OK;
			}
		}
	}

	m_songList.ProcessInput(action, pad);
	switch (*action)
	{
	case INPUT_NONE:
		return S_OK;
	case INPUT_A:
		if (m_nButton == 0)
		{
			m_songList.ToggleMark(m_songList.m_nSelected);
		}
		else if (m_nButton == 1)
		{
			for (UINT i = 0; i < m_songList.m_items.size(); i++)
				m_songList.m_items[i].marked = TRUE;
		}
		else if (m_nButton == 2)
		{
			// Copy, if anything is ticked.
			for (UINT i = 0; i < m_songList.m_items.size(); i++)
			{
				if (m_songList.m_items[i].marked)
				{
					ShowTargetMenu();
					break;
				}
			}
		}
		break;
	case INPUT_B:
	case INPUT_BACK:
		m_bOpen = FALSE;
		break;
	case INPUT_LEFT:
		if (m_nButton > 0)
			m_nButton--;
		break;
	case INPUT_RIGHT:
		if (m_nButton < 3)
			m_nButton++;
		break;
	}
	*action = INPUT_NONE;
	return S_OK;
}

// 0x000F38EF
void CTrackCopy::StartCopy(DWORD target)
{
	if (m_hThread != NULL)
		return;
	m_unusedD0.clear();
	m_dwTarget = target;
	if (m_pBuffer == NULL)
	{
		m_pBuffer = new BYTE[COPY_BUFFER_SIZE];
		if (m_pBuffer == NULL)
			return;
	}
	DWORD threadId;
	m_hThread = CreateThread(NULL, 0, CopyThread, this, 0, &threadId);
	Sleep(50);
}

// 0x000F38BC
// (The thread closes its own handle, so ProcessInput usually finds none.)
DWORD __stdcall CTrackCopy::CopyThread(void *param)
{
	BOOL copied = FALSE;
	if (param != NULL)
	{
		CTrackCopy *self = (CTrackCopy *)param;
		copied = self->CopySelected();
		Sleep(100);
		CloseHandle(self->m_hThread);
		self->m_hThread = NULL;
	}
	return copied;
}

// 0x000F3752
BOOL CTrackCopy::CopySelected()
{
	ShowMessage(GetStringA(STR_MSG_COPYING), GetString(STR_INFORMATION), MSGBOX_BUTTONS_CANCEL);
	// Quirk: the error is only cleared here, so after a song fails every
	// later one fails too.
	m_nError = COPY_OK;
	m_nTrack = 0;
	int copied = 0;
	for (UINT i = 0; i < m_songList.m_items.size(); i++)
	{
		SSongItem &item = m_songList.m_items[i];
		if (item.marked && CopySong(item.index, item.name.c_str(), item.length))
			copied++;
	}
	if (copied != 0)
	{
		m_songList.ClearMarks();
		if (m_bNewSoundtrack && m_pTargetMenu != NULL)
		{
			delete m_pTargetMenu;
			m_pTargetMenu = NULL;
		}
		m_bMessageShown = FALSE;
	}
	else
	{
		std::string text;
		StrFormat(text, GetStringA(STR_MSG_COPY_ERROR), GetLastError());
		ShowMessage(text.c_str(), GetString(STR_ERROR), MSGBOX_BUTTONS_OK);
		// The new soundtrack goes again.
		if (m_bNewSoundtrack && OpenSoundtrackDB())
		{
			DeleteSoundtrack(m_dwTarget);
			CloseSoundtrackDB();
		}
	}
	m_bTargetMenuOpen = FALSE;
	m_bNewSoundtrack = FALSE;
	Sleep(1000);
	delete[] m_pBuffer;
	m_pBuffer = NULL;
	return copied != 0;
}

// 0x000F14C9
BOOL CTrackCopy::CopySong(UINT index, const wchar_t *name, DWORD length)
{
	m_nProgress = 0;
	memset(m_szSongName, 0, sizeof(m_szSongName) / 2);   // (only the first half)
	wcsncpy(m_szSongName, name, 32);
	DWORD songId = 0;
	if (m_dwSoundtrack != 0)
	{
		DWORD songLength;
		wchar_t songName[32];
		XGetSoundtrackSongInfo(m_dwSoundtrack, index, &songId, &songLength, songName, 32);
		length = songLength;
	}
	if (!OpenSoundtrackDB())
		return FALSE;
	DWORD newId = AllocateSongId(m_dwTarget);
	m_nTrack++;
	wchar_t title[34];
	// (Uninitialized in the original when the CD copy stops before the
	// temporary file is made; it's then deleted below.)
	char path[MAX_PATH] = "";

	if (m_dwSoundtrack == 0)
	{
		// The audio CD: the track's raw audio to x:\tempcda.cda, "encoded"
		// to x:\tempwma.wma, which is moved into place.
		swprintf(title, 34, L"%s %d", L"Track", m_nTrack);
		SCddaReader reader;
		CddaReaderOpen(&reader, &g_cdToc, 5);
		DWORD first;
		DWORD end;
		if (g_cdToc.hDevice == INVALID_HANDLE_VALUE)
		{
			first = 0;
			end = 0;
		}
		else
		{
			first = g_cdToc.trackSector[index];
			end = g_cdToc.trackSector[index + 1];
		}
		DWORD sectors = end - first;
		DWORD ms = sectors * 1000 / CD_FRAMES_PER_SECOND;
		if (end < first)
			ms = 0;
		if (g_cdToc.hDevice == INVALID_HANDLE_VALUE)
			m_nError = COPY_NO_DISC;
		else if (ms == 0)
			m_nError = COPY_BAD_TRACK;
		if (m_nError == COPY_OK)
		{
			reader.sector = first;
			reader.bufferPos = 0;
			strcpy(path, TEMP_CDA_PATH);
			HANDLE file = CreateFileA(path, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
			                          FILE_FLAG_SEQUENTIAL_SCAN, NULL);
			if (file == INVALID_HANDLE_VALUE)
			{
				m_nError = COPY_FILE_ERROR;
				CloseSoundtrackDB();
				CddaReaderClose(&reader);
				return FALSE;
			}
			// The whole size up front, if X: has room for it.
			LARGE_INTEGER size;
			size.QuadPart = (LONGLONG)((ULONGLONG)sectors * CD_SECTOR_SIZE);
			if (!SetFilePointerEx(file, size, NULL, FILE_BEGIN) || !SetEndOfFile(file))
				m_nError = COPY_FILE_ERROR;
			ULONGLONG freeBytes;
			if (!GetDiskFreeSpaceExA("X:\\", &freeBytes, NULL, NULL) || freeBytes < (ULONGLONG)size.QuadPart)
				m_nError = COPY_FILE_ERROR;
			SetFilePointer(file, 0, NULL, FILE_BEGIN);
			short peak = -32767;
			DWORD remaining = (DWORD)size.QuadPart;
			while (m_nError == COPY_OK && remaining != 0)
			{
				int read = CddaReaderRead(&reader, m_pBuffer, remaining > COPY_BUFFER_SIZE ? COPY_BUFFER_SIZE : remaining);
				if (read == 0)
					break;
				if (read < 0)
				{
					m_nError = g_cdToc.hDevice != INVALID_HANDLE_VALUE ? COPY_BAD_TRACK : COPY_NO_DISC;
					break;
				}
				DWORD written;
				if (!WriteFile(file, m_pBuffer, read, &written, NULL))
				{
					m_nError = COPY_FILE_ERROR;
					break;
				}
				// The loudest sample, for the encoder's gain.
				const short *sample = (const short *)m_pBuffer;
				for (int n = read / 2; n > 0; n--, sample++)
				{
					short level = *sample;
					if (level < 0)
						level = (short)-level;
					if (level > peak)
						peak = level;
				}
				DWORD done = reader.sector < end ? reader.sector : end;
				done -= first;
				m_nProgress = (int)((float)done / (float)sectors * 100.0f);
				remaining -= read;
			}
			if (!SetEndOfFile(file))
				m_nError = COPY_FILE_ERROR;
			if (!CloseHandle(file))
				m_nError = COPY_FILE_ERROR;
			if (m_nError == COPY_OK)
			{
				m_nError = EncodeTrackToWma(peak) ? COPY_OK : COPY_FILE_ERROR;
				if (m_nError == COPY_OK)
				{
					sprintf(path, "%s%04x\\%08x.wma", SOUNDTRACK_MUSIC_DIR, newId >> 16, newId);
					if (!MoveFileExA(TEMP_WMA_PATH, path, 0))
					{
						// Quirk: the song is still added to ST.DB, without a
						// file (and the stub encoder never makes one).
						DeleteFileA(TEMP_WMA_PATH);
						strcpy(path, TEMP_CDA_PATH);
					}
				}
			}
		}
		Sleep(5);
		CddaReaderClose(&reader);
	}
	else
	{
		// Another soundtrack: a copy of the WMA file. "Track <n>" names are
		// numbered again in the target.
		if (_wcsnicmp(name, L"Track", wcslen(L"Track")) == 0)
			swprintf(title, 34, L"%s %d", L"Track", m_nTrack);
		else
			wcsncpy(title, name, 32);
		char source[MAX_PATH];
		sprintf(source, "%s%04x\\%08x.wma", SOUNDTRACK_MUSIC_DIR, m_dwSoundtrack, songId);
		sprintf(path, "%s%04x\\%08x.wma", SOUNDTRACK_MUSIC_DIR, m_dwTarget, newId);
		HANDLE in = CreateFileA(source, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
		if (in == INVALID_HANDLE_VALUE)
		{
			m_nError = COPY_FILE_ERROR;
			CloseSoundtrackDB();
			return FALSE;
		}
		HANDLE out = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
		if (out == INVALID_HANDLE_VALUE)
		{
			CloseHandle(in);
			m_nError = COPY_FILE_ERROR;
			CloseSoundtrackDB();
			return FALSE;
		}
		DWORD size = GetFileSize(in, NULL);
		LARGE_INTEGER end;
		end.QuadPart = size;
		if (!SetFilePointerEx(out, end, NULL, FILE_BEGIN) || !SetEndOfFile(out))
			m_nError = COPY_FILE_ERROR;
		SetFilePointer(out, 0, NULL, FILE_BEGIN);
		if (m_nError == COPY_OK)
		{
			DWORD total = 0;
			DWORD read;
			BOOL ok = FALSE;
			while (ReadFile(in, m_pBuffer, COPY_BUFFER_SIZE, &read, NULL))
			{
				if (read == 0)
				{
					ok = TRUE;
					break;
				}
				DWORD written;
				if (!WriteFile(out, m_pBuffer, read, &written, NULL))
					break;
				total += read;
				m_nProgress = (int)((float)total / (float)size * 100.0f);
			}
			if (!ok)
				m_nError = COPY_FILE_ERROR;
			if (m_nError == COPY_OK)
			{
				FILETIME created, accessed, written;
				GetFileTime(in, &created, &accessed, &written);
				SetFileTime(out, &created, &accessed, &written);
			}
		}
		CloseHandle(in);
		if (!CloseHandle(out))
			m_nError = COPY_FILE_ERROR;
		m_nProgress = 100;
	}

	if (m_nError == COPY_OK && !AddSong(m_dwTarget, newId, length, title))
		m_nError = COPY_DB_ERROR;
	CloseSoundtrackDB();
	if (m_nError == COPY_OK)
		return TRUE;
	DeleteFileA(path);
	return FALSE;
}

// The encoder's state (0x00270FB8-0x00270FD0).
static DWORD s_dwEncoderBuffered;      // 0x00270FB8 bytes in s_pEncoderBuffer
static short *s_pGainTable;           // 0x00270FBC
static DWORD s_dwEncoderBufferStart;   // 0x00270FC0 their offset in the file
static HANDLE s_hEncoderInput;        // 0x00270FC4
static BYTE *s_pEncoderBuffer;        // 0x00270FC8
static DWORD s_dwEncoderPosition;      // 0x00270FCC
static DWORD s_dwEncoderInputSize;    // 0x00270FD0

// 0x000D9040
// The encoder's PCM source, left behind (nothing calls it): samples
// 16-bit stereo samples from position, through the gain table, buffered.
// (samples in ecx)
static DWORD EncoderReadPcm(DWORD samples, BYTE **out, DWORD position)
{
	s_dwEncoderPosition = position * 4;
	DWORD bytes = samples * 4;
	// Keep the part of the buffer from the position on.
	if (s_dwEncoderPosition < s_dwEncoderBufferStart ||
	    s_dwEncoderBuffered + s_dwEncoderBufferStart <= s_dwEncoderPosition)
	{
		s_dwEncoderBuffered = 0;
	}
	else
	{
		DWORD skip = s_dwEncoderPosition - s_dwEncoderBufferStart;
		s_dwEncoderBuffered -= skip;
		memmove(s_pEncoderBuffer, s_pEncoderBuffer + skip, s_dwEncoderBuffered);
	}
	s_dwEncoderBufferStart = s_dwEncoderPosition;
	if (s_dwEncoderBuffered < bytes)
	{
		DWORD read;
		short *samplesRead = (short *)(s_pEncoderBuffer + s_dwEncoderBuffered);
		if (SetFilePointer(s_hEncoderInput, s_dwEncoderBuffered + s_dwEncoderPosition, NULL, FILE_BEGIN) ==
		        (DWORD)-1 ||
		    !ReadFile(s_hEncoderInput, samplesRead, bytes - s_dwEncoderBuffered, &read, NULL))
			return 0;
		read &= ~1;
		s_dwEncoderBuffered += read;
		if (s_pGainTable != NULL)
		{
			for (DWORD i = 0; i < read; i += 2, samplesRead++)
				*samplesRead = s_pGainTable[*samplesRead + 0x8000];
		}
	}
	DWORD copy = bytes < s_dwEncoderBuffered ? bytes : s_dwEncoderBuffered;
	memcpy(*out, s_pEncoderBuffer, copy);
	return copy;
}

// 0x000D9155
BOOL EncodeTrackToWma(short peak)
{
	char input[MAX_PATH];
	char output[MAX_PATH];
	strcpy(input, TEMP_CDA_PATH);
	strcpy(output, TEMP_WMA_PATH);   // (never written)
	s_hEncoderInput = CreateFileA(input, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
	if (s_hEncoderInput == INVALID_HANDLE_VALUE)
		return FALSE;
	s_dwEncoderInputSize = GetFileSize(s_hEncoderInput, NULL);
	// Quirk: an uninitialized local in the original (it goes on only when
	// that happens to be 0, and is used as both buffers' size).
	DWORD bufferSize = 0;
	if (bufferSize != 0)
	{
		s_dwEncoderInputSize = 0;
		return FALSE;
	}
	BYTE *work = new BYTE[bufferSize];
	if (work == NULL)
	{
		s_dwEncoderInputSize = 0;
		CloseHandle(s_hEncoderInput);
		return FALSE;
	}
	BOOL ok = FALSE;
	s_dwEncoderBufferStart = 0;
	s_dwEncoderBuffered = 0;
	s_pEncoderBuffer = new BYTE[bufferSize];
	if (s_pEncoderBuffer == NULL)
	{
		s_dwEncoderInputSize = 0;
		CloseHandle(s_hEncoderInput);
	}
	else
	{
		// A gain table that would bring the peak to full scale.
		if (peak > 0 && peak < 0x7FFF)
		{
			s_pGainTable = new short[0x10000];
			if (s_pGainTable != NULL)
			{
				float gain = 32767.0f / (float)peak;
				for (int i = 0; i < 0x10000; i++)
					s_pGainTable[i] = (short)(int)((float)(i - 0x8000) * (1.0f / 32767.0f) * gain * 32767.0f);
			}
		}
		// (The encoding itself isn't there.)
		s_dwEncoderInputSize = 0;
		s_dwEncoderPosition = 0;
		if (s_pGainTable != NULL)
		{
			delete[] s_pGainTable;
			s_pGainTable = NULL;
		}
		if (s_pEncoderBuffer != NULL)
		{
			delete[] s_pEncoderBuffer;
			s_pEncoderBuffer = NULL;
		}
		CloseHandle(s_hEncoderInput);
		DeleteFileA(input);
		ok = TRUE;
	}
	delete[] work;
	return ok;
}
