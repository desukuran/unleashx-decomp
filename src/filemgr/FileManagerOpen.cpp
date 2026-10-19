// What the file manager does with a file: launch it, or open it in the
// matching viewer. See FileFolder.h (FolderItemType).
#include "FileManager.h"

#include "../app/UnleashX.h"
#include "../audio/MusicManager.h"
#include "../common/StrUtil.h"
#include "../media/WMVideoPlayer.h"
#include "../musicplayer/AudioGUI.h"
#include "Viewers.h"

// 0x0012C480
void CFileFolderEventsink::OnOpen(std::string path, int type, CXBFontEx *font)
{
	g_bDeleteViewerOnClose = TRUE;
	if (g_pFileViewer != NULL)
	{
		delete g_pFileViewer;
		g_pFileViewer = NULL;
		g_bFileViewerOpen = FALSE;
	}

	switch (type)
	{
	case ITEM_XBE:
		LaunchXbe(path.c_str(), NULL, -1, NULL);
		return;

	case ITEM_AUDIO:
		// The music player, playing just this file.
		if (g_pAudioGUI == NULL)
		{
			g_pAudioGUI = new CAudioGUI;
			if (g_pAudioGUI == NULL)
				return;
			g_pAudioGUI->Init(font);
			if (g_pAudioGUI == NULL)
				return;
		}
		if (g_pAudioGUI->OpenFile(path.c_str()))
		{
			g_pAudioGUI->Show();
			g_bFileAudioPlayer = TRUE;
		}
		return;

	case ITEM_VIDEO:
		if (g_pFileVideoPlayer == NULL)
		{
			g_pFileVideoPlayer = new CWMVideoPlayer;
			if (g_pFileVideoPlayer == NULL)
			{
				g_bFileVideoPlaying = FALSE;
				return;
			}
			g_pFileVideoPlayer->SetFont(font);
			g_pFileVideoPlayer->SetFullScreen(TRUE);
		}
		if (!g_pFileVideoPlayer->Open(path.c_str()))
		{
			// (Quirk: the flag says playing anyway.)
			g_pFileVideoPlayer->Close();
			g_bFileVideoPlaying = TRUE;
			return;
		}
		Sleep(100);
		g_bFileVideoPlaying = TRUE;
		if (g_pMusicManager != NULL)
			g_pMusicManager->Pause();
		// The player is kept (not deleted) when it closes.
		g_bDeleteViewerOnClose = FALSE;
		g_bFileViewerOpen = TRUE;
		g_pFileViewer = g_pFileVideoPlayer;
		return;

	case ITEM_TEXT:
	{
		CNotePad *editor = new CNotePad;
		g_pFileViewer = editor;
		if (editor == NULL)
			return;
		editor->Init(font, NULL, NULL, NULL);
		editor->Open(path.c_str());
		g_bFileTextEditor = TRUE;
		g_bFileViewerOpen = TRUE;
		return;
	}

	case ITEM_IMAGE:
	{
		CImageViewer *viewer = new CImageViewer;
		g_pFileViewer = viewer;
		if (viewer == NULL)
			return;
		viewer->Init(font);
		viewer->Open(path.c_str());
		viewer->Show();
		g_bFileImageViewer = TRUE;
		g_bFileViewerOpen = TRUE;
		return;
	}

	case ITEM_ARCHIVE:
		// By the extension (the type doesn't tell them apart).
		if (StrCmpNoCase(StrRight(path, 4), ".zip") == 0)
		{
			CZipManager *zip = new CZipManager;
			g_pFileViewer = zip;
			if (zip == NULL)
				return;
			zip->Init(font);
			zip->Open(path.c_str());
			g_bFileZipOpen = TRUE;
		}
		else if (StrCmpNoCase(StrRight(path, 4), ".rar") == 0)
		{
			CRarManager *rar = new CRarManager;
			g_pFileViewer = rar;
			if (rar == NULL)
				return;
			rar->Init(font);
			rar->Open(path.c_str());
			g_bFileRarOpen = TRUE;
		}
		else if (StrCmpNoCase(StrRight(path, 4), ".xip") == 0)
		{
			CXipManager *xip = new CXipManager;
			g_pFileViewer = xip;
			if (xip == NULL)
				return;
			xip->Init(font);
			xip->Open(path.c_str());
			g_bFileXipOpen = TRUE;
		}
		else
		{
			return;
		}
		g_bFileViewerOpen = TRUE;
		return;

	case ITEM_MESH:
	{
		CXMViewer *viewer = new CXMViewer;
		g_pFileViewer = viewer;
		if (viewer == NULL)
			return;
		viewer->Init(font);
		viewer->Open(path.c_str());
		viewer->Show();
		g_bFileViewerOpen = TRUE;
		return;
	}
	}
}
