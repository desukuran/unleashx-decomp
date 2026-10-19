# Host build of the portable modules, for syntax checks and tests.
# (The real target is an Xbox XBE built with the XDK; not set up yet.)
CXX ?= g++
CXXFLAGS ?= -std=c++11 -Wall -Wextra -O1 -g

COMMON := src/common/StrUtil.cpp src/common/ObList.cpp src/common/PlatformHost.cpp src/common/FileUtil.cpp
XML := src/xml/XmlDocument.cpp src/xml/XmlUtil.cpp
SETTINGS := src/settings/AppSettings.cpp src/settings/ConfigLoader.cpp
SYSTEM := src/system/Drives.cpp src/system/Dvd.cpp src/system/SystemInfo.cpp src/system/Xbe.cpp
NET := src/net/Network.cpp src/net/XNetHost.cpp src/net/FtpServer.cpp src/net/FtpConnection.cpp src/net/FtpTransfer.cpp
SKIN := src/skin/Skin.cpp src/skin/SkinText.cpp src/skin/SkinParser.cpp src/skin/SkinColors.cpp src/skin/SkinDefaults.cpp
UI := src/ui/Language.cpp src/ui/MenuScan.cpp
RENDER := src/render/Screen.cpp
INPUT := src/input/Gamepad.cpp

# Everything above, plus stand-ins for what isn't decompiled yet.
LIBS := $(COMMON) $(XML) $(SETTINGS) $(SYSTEM) $(NET) $(SKIN) $(UI) $(RENDER) $(INPUT)
STUBS := tests/stubs.cpp tests/todo_stubs.cpp tests/skin_stubs.cpp tests/fileio_host.cpp
HEADERS := $(wildcard src/*/*.h) src/ui/StringTable.inc

TESTS := build/xml_roundtrip build/config_test build/skin_test build/language_test build/skintext_test build/xbe_test build/menuscan_test build/ftp_test build/fileutil_test

# Syntax-checked but not linked: hardware access, or too much of what they
# call isn't decompiled yet.
XBOX_ONLY := src/system/Hardware.cpp src/system/SystemControl.cpp src/skin/SkinLoader.cpp \
	src/app/Main.cpp src/app/MenuActions.cpp src/app/UnleashX.cpp src/app/FtpEvents.cpp src/app/NetworkThread.cpp src/render/Draw.cpp src/ui/MenuItem.cpp src/ui/Menu.cpp \
	src/app/SettingsBase.cpp src/app/SettingsVideo.cpp src/app/SettingsDateTime.cpp src/app/SettingsAutoLaunch.cpp src/app/SettingsQuickLaunch.cpp src/app/SettingsScreenAdjust.cpp src/app/SettingsLiveUpdate.cpp src/app/SettingsNetwork.cpp src/app/HddLockDialog.cpp src/app/SettingsHDD.cpp src/app/SettingsMusic.cpp src/app/SettingsSystem.cpp src/app/SettingsMenu.cpp src/app/IPAddressEditor.cpp src/ui/UpDown.cpp src/app/ClockEditor.cpp src/app/XboxInfo.cpp src/app/MessageBox.cpp src/system/XSettings.cpp

all: $(TESTS) check

check:
	$(CXX) $(CXXFLAGS) -fsyntax-only $(XBOX_ONLY)

build/%: tests/%.cpp $(LIBS) $(STUBS) $(HEADERS)
	@mkdir -p build
	$(CXX) $(CXXFLAGS) -o $@ $< $(LIBS) $(STUBS) -pthread

test: $(TESTS)
	build/xml_roundtrip UnleashX_0_39_0528A/Config.xml > /dev/null
	build/xml_roundtrip UnleashX_0_39_0528A/English.xml > /dev/null
	rm -rf build/testroot && mkdir -p build/testroot/E/dash
	cp UnleashX_0_39_0528A/Config.xml build/testroot/E/dash/config.xml
	UNLEASHX_ROOT=build/testroot build/config_test
	build/skin_test build/skin
	build/language_test UnleashX_0_39_0528A/French.xml
	build/skintext_test
	build/xbe_test
	build/fileutil_test
	rm -rf build/testroot/E/Games && mkdir -p build/testroot/E/Games/A build/testroot/E/Games/B
	cp UnleashX_0_39_0528A/Default.xbe build/testroot/E/Games/A/default.xbe
	cp UnleashX_0_39_0528A/Default.xbe build/testroot/E/Games/B/default.xbe
	touch build/testroot/E/Games/B/icon.png
	UNLEASHX_ROOT=build/testroot build/menuscan_test > build/menuscan.log || (cat build/menuscan.log; false)
	mkdir -p build/testroot/E/ftp && printf 'Hello, Xbox!' > build/testroot/E/ftp/hello.txt
	UNLEASHX_ROOT=build/testroot build/ftp_test > build/ftp.log 2>&1 || (cat build/ftp.log; false)
	@echo ok

.PHONY: all test check
