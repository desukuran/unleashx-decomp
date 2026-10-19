# Host build of the portable modules, for syntax checks and tests.
# (The real target is an Xbox XBE built with the XDK; not set up yet.)
CXX ?= g++
CXXFLAGS ?= -std=c++11 -Wall -Wextra -O1 -g

COMMON := src/common/StrUtil.cpp src/common/ObList.cpp src/common/PlatformHost.cpp
XML := src/xml/XmlDocument.cpp src/xml/XmlUtil.cpp
SETTINGS := src/settings/AppSettings.cpp src/settings/ConfigLoader.cpp src/common/FileUtil.cpp src/system/Drives.cpp

build/xml_roundtrip: tests/xml_roundtrip.cpp $(XML) $(COMMON) $(wildcard src/*/*.h)
	@mkdir -p build
	$(CXX) $(CXXFLAGS) -o $@ tests/xml_roundtrip.cpp $(XML) $(COMMON)

build/config_test: tests/config_test.cpp tests/stubs.cpp $(SETTINGS) $(XML) $(COMMON) $(wildcard src/*/*.h)
	@mkdir -p build
	$(CXX) $(CXXFLAGS) -o $@ tests/config_test.cpp tests/stubs.cpp $(SETTINGS) $(XML) $(COMMON)

test: build/xml_roundtrip build/config_test
	build/xml_roundtrip UnleashX_0_39_0528A/Config.xml > /dev/null
	build/xml_roundtrip UnleashX_0_39_0528A/English.xml > /dev/null
	rm -rf build/testroot && mkdir -p build/testroot/E/dash
	cp UnleashX_0_39_0528A/Config.xml build/testroot/E/dash/config.xml
	UNLEASHX_ROOT=build/testroot build/config_test
	@echo ok

.PHONY: test
