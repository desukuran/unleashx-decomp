# Host build of the portable modules, for syntax checks and tests.
# (The real target is an Xbox XBE built with the XDK; not set up yet.)
CXX ?= g++
CXXFLAGS ?= -std=c++11 -Wall -Wextra -O1 -g

COMMON := src/common/StrUtil.cpp src/common/ObList.cpp src/common/PlatformHost.cpp src/common/FileUtil.cpp
XML := src/xml/XmlDocument.cpp src/xml/XmlUtil.cpp
SETTINGS := src/settings/AppSettings.cpp src/settings/ConfigLoader.cpp
SYSTEM := src/system/Drives.cpp
SKIN := src/skin/Skin.cpp src/skin/SkinParser.cpp src/skin/SkinColors.cpp src/skin/SkinDefaults.cpp
UI := src/ui/Language.cpp

# Everything above, plus stand-ins for what isn't decompiled yet.
LIBS := $(COMMON) $(XML) $(SETTINGS) $(SYSTEM) $(SKIN) $(UI)
STUBS := tests/stubs.cpp tests/todo_stubs.cpp tests/skin_stubs.cpp
HEADERS := $(wildcard src/*/*.h) src/ui/StringTable.inc

TESTS := build/xml_roundtrip build/config_test build/skin_test build/language_test

all: $(TESTS)

build/%: tests/%.cpp $(LIBS) $(STUBS) $(HEADERS)
	@mkdir -p build
	$(CXX) $(CXXFLAGS) -o $@ $< $(LIBS) $(STUBS)

test: $(TESTS)
	build/xml_roundtrip UnleashX_0_39_0528A/Config.xml > /dev/null
	build/xml_roundtrip UnleashX_0_39_0528A/English.xml > /dev/null
	rm -rf build/testroot && mkdir -p build/testroot/E/dash
	cp UnleashX_0_39_0528A/Config.xml build/testroot/E/dash/config.xml
	UNLEASHX_ROOT=build/testroot build/config_test
	build/skin_test build/skin
	build/language_test UnleashX_0_39_0528A/French.xml
	@echo ok

.PHONY: all test
