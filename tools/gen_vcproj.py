#!/usr/bin/env python3
"""Generates vs2003/UnleashX.sln and vs2003/UnleashX.vcproj: the Xbox build
for Visual Studio .NET 2003 with the XDK installed, from the same sources and
flags as Makefile.xbox. Rerun after adding or removing source files, or
after changing assets/media.zip or assets/internal_skins.zip (their sizes are
compiled in).

  python3 tools/gen_vcproj.py
"""
import glob
import os
import re
import uuid

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
OUTDIR = os.path.join(ROOT, 'vs2003')

PROJECT_GUID = '{8E4C2A1B-5D3F-4C7E-9A21-0B5E7A3D9C10}'
VCPROJ_TYPE = '{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}'

# Kept in step with Makefile.xbox.
DEFINES = '_XBOX;NDEBUG;WIN32;DEBUG_MOUSE'
LIBS = ('xapilib.lib d3d8.lib d3dx8.lib xgraphics.lib dsound.lib xacteng.lib xmv.lib xnet.lib '
        'xboxkrnl.lib xkbd.lib libcmt.lib libcpmt.lib')
MAD_SRCS = 'bit decoder fixed frame huffman layer12 layer3 stream synth timer version'.split()
IMAGEBLD_FLAGS = ('/NOLOGO /NOLIBWARN /STACK:0x40000 /INITFLAGS:0x1C /TESTID:0x9E115330 '
                  '&quot;/TESTNAME:UnleashX Xbox Launcher&quot; /TESTMEDIATYPES:0x800003FF '
                  '/TESTREGION:0x80000007 /TESTRATINGS:0xFFFFFFFF')


def rel(path):
    return '..\\' + os.path.relpath(path, ROOT).replace('/', '\\')


def rar_sources():
    # The files UnrarXLib's own project compiles (Makefile.xbox's RAR_SRCS).
    text = open(os.path.join(ROOT, 'Makefile.xbox')).read()
    m = re.search(r'RAR_SRCS := ((?:[^\n]*\\\n)*[^\n]*)', text)
    return m.group(1).replace('\\\n', ' ').split()


def file_entry(path, settings=()):
    """A file; .cpp/.c files get an object name with their folder in it, as
    some names repeat (src/system/Md5.cpp, src/thirdparty/Md5.cpp)."""
    if not path.endswith(('.cpp', '.c')):
        return '\t\t\t<File RelativePath="%s"/>\n' % rel(path)
    folder = os.path.relpath(os.path.dirname(path), ROOT).replace('/', '_').replace('\\', '_')
    obj = os.path.splitext(os.path.basename(path))[0]
    settings = [kv for kv in settings if kv[0] != 'ObjectFile']
    settings.append(('ObjectFile', '$(IntDir)/%s_%s.obj' % (folder, obj)))
    attrs = ' '.join('%s="%s"' % kv for kv in settings)
    return ('\t\t\t<File RelativePath="%s">\n'
            '\t\t\t\t<FileConfiguration Name="Release|Xbox">\n'
            '\t\t\t\t\t<Tool Name="VCCLCompilerTool" %s/>\n'
            '\t\t\t\t</FileConfiguration>\n'
            '\t\t\t</File>\n') % (rel(path), attrs)


def main():
    media = os.path.getsize(os.path.join(ROOT, 'assets/media.zip'))
    skins = os.path.getsize(os.path.join(ROOT, 'assets/internal_skins.zip'))
    defines = '%s;MEDIA_FILE_SIZE=%d;INT_SKINS_SIZE=%d' % (DEFINES, media, skins)
    host_only = {'src/common/PlatformHost.cpp', 'src/net/XNetHost.cpp'}

    groups = []
    for sub in sorted(os.listdir(os.path.join(ROOT, 'src'))):
        cpps = sorted(glob.glob(os.path.join(ROOT, 'src', sub, '*.cpp')))
        hs = sorted(glob.glob(os.path.join(ROOT, 'src', sub, '*.h')))
        entries = [file_entry(p) for p in cpps if os.path.relpath(p, ROOT) not in host_only]
        entries += [file_entry(p) for p in hs]
        if entries:
            groups.append(('src\\' + sub, entries))
    groups.append(('xbox', [file_entry(p) for p in sorted(glob.glob(os.path.join(ROOT, 'xbox', '*.cpp')))]))

    # The third-party files keep their own flags, as in Makefile.xbox.
    tp = os.path.join(ROOT, 'thirdparty')
    base = [('WarningLevel', '0'), ('AdditionalIncludeDirectories', '..\\xbox\\include')]
    tp_entries = [file_entry(os.path.join(tp, 'zip_utils', f), base) for f in ('unzip.cpp', 'zip.cpp')]
    tp_entries.append(file_entry(os.path.join(tp, 'ftpparse', 'ftpparse.c'), base))
    groups.append(('thirdparty\\zip_utils + ftpparse', tp_entries))
    mad = base[:1] + [('AdditionalIncludeDirectories', '..\\xbox\\include;..\\thirdparty\\libmad'),
                      ('PreprocessorDefinitions', '_XBOX;NDEBUG;WIN32;FPM_INTEL;HAVE_CONFIG_H;ASO_ZEROCHECK')]
    groups.append(('thirdparty\\libmad', [file_entry(os.path.join(tp, 'libmad', f + '.c'), mad) for f in MAD_SRCS]))
    rar = base[:1] + [('AdditionalIncludeDirectories', '..\\xbox\\include;..\\thirdparty\\unrar\\shim'),
                      ('PreprocessorDefinitions', '_XBOX;NDEBUG;WIN32;_LIB'),
                      ('ForcedIncludeFiles', 'unrar_xbox.h')]
    groups.append(('thirdparty\\unrar', [file_entry(os.path.join(tp, 'unrar', f + '.cpp'), rar)
                                        for f in rar_sources()]))

    assets = '$(ProjectDir)..\\assets\\'
    post_build = ('copy /Y &quot;{a}media.zip&quot; &quot;$(OutDir)\\MEDIA_FILE.bin&quot; &gt;nul &amp;&amp; '
                  'copy /Y &quot;{a}internal_skins.zip&quot; &quot;$(OutDir)\\INT_SKINS.bin&quot; &gt;nul &amp;&amp; '
                  'cd /d &quot;$(OutDir)&quot; &amp;&amp; '
                  'imagebld.exe {f} /TITLEIMAGE:&quot;{a}title_image.xpr&quot; '
                  '/DEFAULTSAVEIMAGE:&quot;{a}save_image.xpr&quot; '
                  '/INSERTFILE:MEDIA_FILE.bin,MEDIA_FILE,RN '
                  '/INSERTFILE:&quot;{a}default_font.xpr&quot;,DEF_FONT,RN '
                  '/INSERTFILE:INT_SKINS.bin,INT_SKINS,RN /IN:default.exe /OUT:debug.xbe &amp;&amp; '
                  '(python &quot;$(ProjectDir)..\\tools\\xbe.py&quot; retail debug.xbe default.xbe || '
                  'echo Python not found: debug.xbe is the devkit image, run tools\\xbe.py retail for consoles.)'
                  ).format(a=assets, f=IMAGEBLD_FLAGS)

    out = []
    out.append('<?xml version="1.0" encoding="Windows-1252"?>\n')
    out.append('<!-- Generated by tools/gen_vcproj.py from the same lists as Makefile.xbox; '
               'edit that, not this. -->\n')
    out.append('<VisualStudioProject ProjectType="Visual C++" Version="7.10" Name="UnleashX" '
               'ProjectGUID="%s" Keyword="XboxProj">\n' % PROJECT_GUID)
    out.append('\t<Platforms>\n\t\t<Platform Name="Xbox"/>\n\t</Platforms>\n')
    out.append('\t<Configurations>\n')
    out.append('\t\t<Configuration Name="Release|Xbox" OutputDirectory="..\\build\\vs2003" '
               'IntermediateDirectory="..\\build\\vs2003\\obj" ConfigurationType="1">\n')
    out.append('\t\t\t<Tool Name="VCCLCompilerTool" Optimization="2" PreprocessorDefinitions="%s" '
               'StringPooling="TRUE" ExceptionHandling="TRUE" RuntimeLibrary="0" RuntimeTypeInfo="TRUE" '
               'UsePrecompiledHeader="0" '
               'WarningLevel="3" SuppressStartupBanner="TRUE" DisableSpecificWarnings="4996" CompileAs="0"/>\n'
               % defines)
    out.append('\t\t\t<Tool Name="VCLinkerTool" AdditionalOptions="/FIXED:NO" AdditionalDependencies="%s" '
               'OutputFile="$(OutDir)/default.exe" LinkIncremental="1" SuppressStartupBanner="TRUE" '
               'GenerateDebugInformation="TRUE" GenerateMapFile="TRUE" MapFileName="$(OutDir)/default.map"/>\n'
               % LIBS)
    out.append('\t\t\t<Tool Name="VCPostBuildEventTool" Description="Building default.xbe with UnleashX\'s '
               'resources" CommandLine="%s"/>\n' % post_build)
    out.append('\t\t\t<Tool Name="XboxImageTool" SuppressStartupBanner="TRUE" TitleID="0x9E115330" '
               'TitleName="UnleashX Xbox Launcher" StackSize="0x40000" NoLibWarn="TRUE"/>\n')
    out.append('\t\t\t<Tool Name="XboxDeploymentTool" RemotePath="xe:\\UnleashX\\default.xbe"/>\n')
    out.append('\t\t</Configuration>\n\t</Configurations>\n\t<Files>\n')
    for name, entries in groups:
        out.append('\t\t<Filter Name="%s">\n' % name)
        out.extend(entries)
        out.append('\t\t</Filter>\n')
    out.append('\t</Files>\n\t<Globals/>\n</VisualStudioProject>\n')

    os.makedirs(OUTDIR, exist_ok=True)
    with open(os.path.join(OUTDIR, 'UnleashX.vcproj'), 'w', newline='\r\n') as f:
        f.write(''.join(out))

    sln = ('Microsoft Visual Studio Solution File, Format Version 8.00\n'
           'Project("%s") = "UnleashX", "UnleashX.vcproj", "%s"\n'
           '\tProjectSection(ProjectDependencies) = postProject\n\tEndProjectSection\n'
           'EndProject\n'
           'Global\n'
           '\tGlobalSection(SolutionConfiguration) = preSolution\n\t\tRelease = Release\n\tEndGlobalSection\n'
           '\tGlobalSection(ProjectConfiguration) = postSolution\n'
           '\t\t%s.Release.ActiveCfg = Release|Xbox\n\t\t%s.Release.Build.0 = Release|Xbox\n'
           '\tEndGlobalSection\n'
           '\tGlobalSection(ExtensibilityGlobals) = postSolution\n\tEndGlobalSection\n'
           '\tGlobalSection(ExtensibilityAddIns) = postSolution\n\tEndGlobalSection\n'
           'EndGlobal\n') % (VCPROJ_TYPE, PROJECT_GUID, PROJECT_GUID, PROJECT_GUID)
    with open(os.path.join(OUTDIR, 'UnleashX.sln'), 'w', newline='\r\n') as f:
        f.write('﻿' + sln)
    print('wrote vs2003/UnleashX.sln and vs2003/UnleashX.vcproj (%d files)' %
          sum(len(e) for _, e in groups))


if __name__ == '__main__':
    main()
