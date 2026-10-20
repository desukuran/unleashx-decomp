# Host build of the portable modules, for syntax checks and tests. The
# Xbox build is Makefile.xbox (or "make xbox XDK=..." / "make xiso XDK=...").
CXX ?= g++
# The original deletes some polymorphic objects through classes without a
# virtual destructor (CXBFont, CMusicManager); that's kept on purpose.
CXXFLAGS ?= -std=c++11 -Wall -Wextra -Wno-delete-non-virtual-dtor -O1 -g

COMMON := src/common/Base64.cpp src/common/StrUtil.cpp src/common/ObList.cpp src/common/PlatformHost.cpp src/common/FileUtil.cpp
XML := src/xml/XmlDocument.cpp src/xml/XmlUtil.cpp
SETTINGS := src/settings/AppSettings.cpp src/settings/ConfigLoader.cpp
SYSTEM := src/system/Md5.cpp src/thirdparty/Md5.cpp src/system/Drives.cpp src/system/Dvd.cpp src/system/SystemInfo.cpp src/system/Xbe.cpp src/system/XbePatch.cpp src/system/XipArchive.cpp src/system/FileOpsState.cpp src/system/FileList.cpp
NET := src/net/Newsfeed.cpp src/net/Network.cpp src/net/XNetHost.cpp src/net/FtpServer.cpp src/net/FtpConnection.cpp src/net/FtpTransfer.cpp src/net/Socket.cpp src/net/Http.cpp src/net/Sntp.cpp src/net/FtpClient.cpp
SKIN := src/skin/Skin.cpp src/skin/SkinText.cpp src/skin/SkinParser.cpp src/skin/SkinColors.cpp src/skin/SkinDefaults.cpp
UI := src/ui/Language.cpp src/ui/MenuScan.cpp
RENDER := src/render/Screen.cpp src/render/FontTable.cpp
INPUT := src/input/Gamepad.cpp
AUDIO := src/audio/SoundtrackDB.cpp

# Everything above, plus stand-ins for what isn't decompiled yet.
LIBS := $(COMMON) $(XML) $(SETTINGS) $(SYSTEM) $(NET) $(SKIN) $(UI) $(RENDER) $(INPUT) $(AUDIO)
STUBS := tests/stubs.cpp tests/todo_stubs.cpp tests/skin_stubs.cpp tests/fileio_host.cpp tests/ftpparse_host.cpp tests/ftplog_host.cpp
HEADERS := $(wildcard src/*/*.h) src/ui/StringTable.inc

TESTS := build/xbepatch_test build/xml_roundtrip build/config_test build/skin_test build/language_test build/skintext_test build/xbe_test build/menuscan_test build/ftp_test build/fileutil_test build/xip_test build/http_test build/ftpclient_test build/soundtrackdb_test build/newsfeed_test build/fonttable_test

# Syntax-checked but not linked: hardware access, or too much of what they
# call isn't decompiled yet.
XBOX_ONLY := src/system/Hardware.cpp src/system/SystemControl.cpp src/skin/SkinLoader.cpp src/skin/SkinPreview.cpp \
	src/app/Main.cpp src/app/MenuActions.cpp src/app/UnleashX.cpp src/app/FtpEvents.cpp src/app/NetworkThread.cpp src/render/Draw.cpp src/ui/MenuItem.cpp src/ui/Menu.cpp \
	src/app/SettingsBase.cpp src/app/SettingsVideo.cpp src/app/SettingsDateTime.cpp src/app/SettingsAutoLaunch.cpp src/app/SettingsQuickLaunch.cpp src/app/SettingsScreenAdjust.cpp src/app/SettingsLiveUpdate.cpp src/app/SettingsNetwork.cpp src/app/HddLockDialog.cpp src/app/SettingsHDD.cpp src/app/SettingsMusic.cpp src/app/SettingsSystem.cpp src/app/SettingsMenu.cpp src/app/IPAddressEditor.cpp src/ui/UpDown.cpp src/ui/OptionMenu.cpp src/ui/DropDown.cpp src/app/ClockEditor.cpp src/app/XboxInfo.cpp src/app/MessageBox.cpp src/app/FrameMove.cpp src/app/AppRender.cpp src/app/Initialize.cpp src/app/Resources.cpp src/app/Startup.cpp src/app/ScreenSaver.cpp src/app/Background.cpp src/ui/VirtualKeyboard.cpp src/ui/PassCode.cpp src/app/NotePad.cpp src/ui/FileBrowser.cpp src/app/SlideShow.cpp src/app/SkinBrowser.cpp src/net/HttpProgress.cpp src/system/FileHDD.cpp src/system/FileISO9660.cpp src/system/FileFAT32.cpp src/system/FileIO.cpp src/system/FileOps.cpp src/audio/MusicManager.cpp src/common/Thread.cpp src/system/XSettings.cpp src/filemgr/FileFolder.cpp src/filemgr/FileManager.cpp src/filemgr/FileManagerOpen.cpp src/filemgr/FileManagerOps.cpp src/filemgr/FileManagerInput.cpp src/filemgr/ZipManager.cpp src/filemgr/XipManager.cpp src/filemgr/RarManager.cpp src/filemgr/XMViewer.cpp src/filemgr/ImageViewer.cpp src/filemgr/FtpLog.cpp src/filemgr/FtpSite.cpp src/filemgr/FtpQueue.cpp src/filemgr/FtpServerList.cpp src/filemgr/FtpFileFolder.cpp src/filemgr/FtpManager.cpp src/filemgr/FtpManagerInput.cpp src/render/Texture.cpp src/audio/Sound.cpp src/skin/SkinRender.cpp \
	src/musicplayer/SongList.cpp src/musicplayer/AudioGUI.cpp src/musicplayer/AudioGUIRender.cpp src/musicplayer/AudioGUIInput.cpp src/app/MenuItemEditor.cpp src/ui/MenuRender.cpp src/app/GameManager.cpp src/app/GameManagerRender.cpp src/render/ImageManager.cpp src/musicplayer/TrackOptions.cpp src/musicplayer/TrackEditor.cpp src/musicplayer/TrackCopy.cpp src/audio/AudioDevice.cpp src/audio/AudioPump.cpp src/audio/CdAudio.cpp src/audio/WavPlayer.cpp src/audio/CdPlayer.cpp src/audio/WmaPlayer.cpp src/audio/Mp3Player.cpp src/audio/Mp3Decoder.cpp src/common/FileLoad.cpp src/audio/SoundEffects.cpp src/media/WMVideoPlayer.cpp \
	src/system/XbeLaunch.cpp src/app/XBAppRun.cpp src/audio/DashUtil.cpp src/system/Storage.cpp src/system/DvdPoll.cpp src/app/LaunchScreen.cpp src/render/XbeImage.cpp src/render/FontEx.cpp src/render/ProgressBox.cpp src/render/TextureTable.cpp

all: $(TESTS) check

check:
	$(CXX) $(CXXFLAGS) -fsyntax-only $(XBOX_ONLY)

build/%: tests/%.cpp $(LIBS) $(STUBS) $(HEADERS)
	@mkdir -p build
	$(CXX) $(CXXFLAGS) -o $@ $< $(LIBS) $(STUBS) -pthread

# The ISO 9660 reader from the Xbox build, on a generated disc image.
build/iso9660_test: tests/iso9660_test.cpp xbox/Iso9660.cpp $(HEADERS)
	@mkdir -p build
	$(CXX) $(CXXFLAGS) -o $@ tests/iso9660_test.cpp xbox/Iso9660.cpp src/common/PlatformHost.cpp -pthread

build/testdisc.iso:
	rm -rf build/testdisc && mkdir -p build/testdisc/Folder
	printf 'Hello, ISO!\n' > build/testdisc/README.TXT
	python3 -c "open('build/testdisc/Folder/Big.bin','wb').write(bytes(i & 0xFF for i in range(5000)))"
	xorriso -as mkisofs -quiet -J -V TESTDISC -o $@ build/testdisc

# The tests that read the release's Default.xbe are skipped without it.
RELEASE_XBE ?= UnleashX_0_39_0528A/Default.xbe
HAVE_RELEASE := $(wildcard $(RELEASE_XBE))

test: $(TESTS)
	build/xml_roundtrip assets/disc/Config.xml > /dev/null
	build/xml_roundtrip assets/disc/English.xml > /dev/null
	rm -rf build/testroot && mkdir -p build/testroot/E/dash
	cp assets/disc/Config.xml build/testroot/E/dash/config.xml
	UNLEASHX_ROOT=build/testroot build/config_test
	rm -rf build/skin && python3 -c "import zipfile; zipfile.ZipFile('assets/disc/Skins/MetallicAtmosphere/MetallicAtmosphere.zip').extractall('build/skin')"
	build/skin_test build/skin
	build/language_test assets/disc/French.xml
	build/skintext_test
	$(if $(HAVE_RELEASE),build/xbe_test $(RELEASE_XBE),@echo "(xbe_test skipped: no $(RELEASE_XBE))")
	build/fileutil_test
	build/xip_test
	rm -rf build/testroot/Y && mkdir -p build/testroot/Y
	UNLEASHX_ROOT=build/testroot build/http_test
ifneq ($(HAVE_RELEASE),)
	rm -rf build/testroot/E/Games && mkdir -p build/testroot/E/Games/A build/testroot/E/Games/B
	cp $(RELEASE_XBE) build/testroot/E/Games/A/default.xbe
	cp $(RELEASE_XBE) build/testroot/E/Games/B/default.xbe
	touch build/testroot/E/Games/B/icon.png
	UNLEASHX_ROOT=build/testroot build/menuscan_test > build/menuscan.log || (cat build/menuscan.log; false)
else
	@echo "(menuscan_test skipped: no $(RELEASE_XBE))"
endif
	mkdir -p build/testroot/E/ftp && printf 'Hello, Xbox!' > build/testroot/E/ftp/hello.txt
	UNLEASHX_ROOT=build/testroot build/ftp_test > build/ftp.log 2>&1 || (cat build/ftp.log; false)
	rm -rf build/testroot/E/client build/testroot/E/ftp/tree build/testroot/E/ftp/made
	UNLEASHX_ROOT=build/testroot build/ftpclient_test > build/ftpclient.log 2>&1 || (cat build/ftpclient.log; false)
	rm -rf build/testroot/E/TDATA
	UNLEASHX_ROOT=build/testroot build/soundtrackdb_test
	build/newsfeed_test
	build/fonttable_test
	$(if $(HAVE_RELEASE),build/xbepatch_test,@echo "(xbepatch_test skipped: no $(RELEASE_XBE))")
	@if command -v xorriso >/dev/null; then $(MAKE) --no-print-directory build/iso9660_test build/testdisc.iso && \
		build/iso9660_test build/testdisc.iso; else echo "(iso9660_test skipped: no xorriso)"; fi
	@echo ok

# The Xbox build lives in Makefile.xbox; these pass XDK= and the rest on.
xbox:
	$(MAKE) -f Makefile.xbox all
xiso:
	$(MAKE) -f Makefile.xbox xiso

.PHONY: all test check xbox xiso
