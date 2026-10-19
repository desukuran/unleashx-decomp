// Runs the decompiled FTP client (CDirFTP) against the decompiled FTP
// server on localhost: login, listings, uploads and downloads of files and
// folders, MKD, renames, deletes and a folder tree removal. Files live under
// $UNLEASHX_ROOT (drive letters are folders).
#include <arpa/inet.h>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unistd.h>

#include "../src/common/FileUtil.h"
#include "../src/common/XboxKernel.h"
#include "../src/filemgr/FtpLog.h"
#include "../src/net/FtpClient.h"
#include "../src/net/FtpServer.h"
#include "../src/net/Network.h"
#include "../src/settings/AppSettings.h"

static int s_failures;

#define CHECK(cond)                                                          \
	do                                                                       \
	{                                                                        \
		if (!(cond))                                                         \
		{                                                                    \
			fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); \
			s_failures++;                                                    \
		}                                                                    \
	} while (0)

// Counts the errors the client logs.
class CTestLog : public CFTPLog
{
public:
	CTestLog() : m_nErrors(0) {}
	virtual void Add(const char *text, int type)
	{
		printf("[%d] %s\n", type, text);
		if (type == FTPLOG_ERROR)
			m_nErrors++;
	}
	int m_nErrors;
};

static std::string ReadHostFile(const std::string &path)
{
	std::string data;
	FILE *f = ::fopen(path.c_str(), "rb");
	if (f == NULL)
		return "(missing)";
	char buffer[4096];
	size_t n;
	while ((n = fread(buffer, 1, sizeof(buffer), f)) > 0)
		data.append(buffer, n);
	fclose(f);
	return data;
}

static void WriteHostFile(const std::string &path, const std::string &data)
{
	FILE *f = ::fopen(path.c_str(), "wb");
	fwrite(data.data(), 1, data.size(), f);
	fclose(f);
}

// The names in the client's listing of the current directory.
static std::string ListNames(CDirFTP &ftp)
{
	std::string names;
	WIN32_FIND_DATAA data;
	HANDLE find = ftp.FindFirst(ftp.m_strCurrentDir.c_str(), &data);
	if (find != INVALID_HANDLE_VALUE)
	{
		do
		{
			names += data.cFileName;
			if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
				names += "/";
			names += " ";
		} while (ftp.FindNext(find, &data));
	}
	ftp.FindClose(find);
	return names;
}

int main()
{
	signal(SIGPIPE, SIG_IGN);
	const char *root = getenv("UNLEASHX_ROOT");
	std::string base = std::string(root != NULL ? root : ".");

	// What the network thread would have done.
	g_bHostThreads = TRUE;
	g_bNetworkStarted = TRUE;
	g_dwXnAddrStatus = XNET_GET_XNADDR_STATIC | XNET_GET_XNADDR_ETHERNET;
	g_dwIPAddress = htonl(INADDR_LOOPBACK);
	CAppSettings::GetInstance()->m_network.m_bAutoDetect = FALSE;
	int port = 22000 + getpid() % 1000;
	g_ftpServer.SetPort(port);
	CHECK(g_ftpServer.Start());

	// Local files to upload.
	std::string big;
	for (int i = 0; i < 300000; i++)
		big += (char)('a' + i % 26);
	CreateDirectoryTree("E:\\client\\tree\\sub");
	WriteHostFile(base + "/E/client/big.bin", big);
	WriteHostFile(base + "/E/client/tree/one.txt", "one");
	WriteHostFile(base + "/E/client/tree/sub/two.txt", "two two");

	CTestLog log;
	CDirFTP ftp(&log);
	CHECK(ftp.Connect("127.0.0.1", port, "xbox", "xbox"));
	CHECK(ftp.m_bConnected);
	CHECK(ftp.m_bUnleashXServer);
	CHECK(ftp.m_strCurrentDir == "/");
	CHECK(ftp.GetDriveType() == 3);

	// Listing (passive).
	CHECK(ftp.ChangeDir("/E/ftp/"));
	CHECK(ftp.m_strCurrentDir == "/E/ftp/");
	std::string names = ListNames(ftp);
	printf("listing: %s\n", names.c_str());
	CHECK(names.find("hello.txt ") != std::string::npos);
	CHECK(names.find("..") == std::string::npos);
	WIN32_FIND_DATAA data;
	CHECK(ftp.FindFirst("/E/ftp/", &data) != INVALID_HANDLE_VALUE);
	ftp.FindClose((HANDLE)1);

	// A file up and down.
	CHECK(ftp.CopyFileEx("E:\\client\\big.bin", "/E/ftp/big.bin", NULL, NULL, NULL, 0));
	CHECK(ReadHostFile(base + "/E/ftp/big.bin") == big);
	CHECK(ftp.CopyFileEx("/E/ftp/big.bin", "E:\\client\\down\\big.bin", NULL, NULL, NULL, 0));
	CHECK(ReadHostFile(base + "/E/client/down/big.bin") == big);
	CHECK(ftp.m_nFilesUploaded == 1);
	CHECK(ftp.m_nFilesDownloaded == 1);
	CHECK(ftp.m_ullBytesUploaded == big.size());
	CHECK(ftp.m_ullBytesDownloaded == big.size());
	// An empty file is just created.
	WriteHostFile(base + "/E/ftp/empty.txt", "");
	CHECK(ftp.CopyFile("/E/ftp/empty.txt", "E:\\client\\empty.txt", FALSE));
	CHECK(ReadHostFile(base + "/E/client/empty.txt") == "");

	// Folders, renames, deletes.
	CHECK(ftp.CreateDirectory("made", NULL));
	CHECK(ftp.MoveFile("/E/ftp/big.bin", "/E/ftp/made/moved.bin"));
	CHECK(ReadHostFile(base + "/E/ftp/made/moved.bin") == big);
	CHECK(ftp.DeleteFile("/E/ftp/made/moved.bin"));
	CHECK(ReadHostFile(base + "/E/ftp/made/moved.bin") == "(missing)");
	CHECK(!ftp.DeleteFile("/E/ftp/made/moved.bin"));

	// A folder up, the way for servers that need MKD.
	ftp.m_bUnleashXServer = FALSE;
	CHECK(ftp.CopyFileEx("E:\\client\\tree", "/E/ftp/tree", NULL, NULL, NULL, COPY_FTP_FOLDER));
	CHECK(ReadHostFile(base + "/E/ftp/tree/one.txt") == "one");
	CHECK(ReadHostFile(base + "/E/ftp/tree/sub/two.txt") == "two two");
	ftp.m_bUnleashXServer = TRUE;

	// ... and down again.
	CHECK(ftp.CopyFileEx("/E/ftp/tree/", "E:\\client\\back", NULL, NULL, NULL, 0));
	CHECK(ReadHostFile(base + "/E/client/back/one.txt") == "one");
	CHECK(ReadHostFile(base + "/E/client/back/sub/two.txt") == "two two");

	// The whole tree away.
	CHECK(ftp.ChangeDir("/E/ftp/"));
	CHECK(ftp.RemoveDirectory("/E/ftp/tree"));
	CHECK(ReadHostFile(base + "/E/ftp/tree/one.txt") == "(missing)");
	CHECK(GetFileAttributesA("E:\\ftp\\tree") == INVALID_FILE_ATTRIBUTES);
	CHECK(ftp.m_strCurrentDir == "/E/ftp/");

	// CDUP and active mode.
	CHECK(ftp.ChangeDir("CDUP"));
	CHECK(ftp.m_strCurrentDir == "/E/");
	ftp.m_bPassive = FALSE;
	ftp.SetMinPort(23000 + getpid() % 1000);
	CHECK(ftp.ChangeDir("ftp"));
	names = ListNames(ftp);
	printf("active listing: %s\n", names.c_str());
	CHECK(names.find("hello.txt ") != std::string::npos);
	CHECK(names.find("made/ ") != std::string::npos);

	// The DELE of the missing file (and a 502 when the server's random
	// PASV port can't be bound; the client then goes active).
	CHECK(log.m_nErrors >= 1);
	ftp.Disconnect();
	CHECK(!ftp.m_bConnected);

	Sleep(500);
	g_ftpServer.Stop();
	Sleep(2000);   // let the server's threads finish
	printf(s_failures == 0 ? "ftpclient_test passed\n" : "ftpclient_test: %d failures\n", s_failures);
	fflush(stdout);
	_exit(s_failures == 0 ? 0 : 1);
}
