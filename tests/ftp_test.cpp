// Runs the decompiled FTP server on localhost and drives a session with a
// small client: login, directories, PASV listings and transfers, renames.
// Files live under $UNLEASHX_ROOT (drive letters are folders).
#include <arpa/inet.h>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include "../src/common/XboxKernel.h"
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

static int Connect(int port)
{
	int s = socket(AF_INET, SOCK_STREAM, 0);
	timeval timeout = {10, 0};
	setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
	sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	if (connect(s, (sockaddr *)&addr, sizeof(addr)) != 0)
	{
		close(s);
		return -1;
	}
	return s;
}

// The next complete reply ("ddd text"), skipping "ddd-" continuation lines
// and anything else before it.
static std::string ReadReply(int s)
{
	static std::string pending;
	for (;;)
	{
		size_t end;
		while ((end = pending.find("\r\n")) != std::string::npos)
		{
			std::string line = pending.substr(0, end);
			pending.erase(0, end + 2);
			if (line.size() >= 4 && isdigit(line[0]) && isdigit(line[1]) && isdigit(line[2]) && line[3] == ' ')
				return line;
		}
		char buffer[4096];
		int n = recv(s, buffer, sizeof(buffer), 0);
		if (n <= 0)
			return "(connection closed)";
		pending.append(buffer, n);
	}
}

static std::string Command(int s, const char *text)
{
	std::string line = text;
	line += "\r\n";
	send(s, line.c_str(), line.size(), 0);
	std::string reply = ReadReply(s);
	printf("> %s\n< %s\n", text, reply.c_str());
	return reply;
}

static bool StartsWith(const std::string &s, const char *prefix)
{
	return s.compare(0, strlen(prefix), prefix) == 0;
}

// PASV, then the data connection.
static int OpenPassive(int control)
{
	std::string reply = Command(control, "PASV");
	if (!StartsWith(reply, "227 "))
		return -1;
	int a, b, c, d, p1, p2;
	if (sscanf(reply.c_str() + reply.find('(') + 1, "%d,%d,%d,%d,%d,%d", &a, &b, &c, &d, &p1, &p2) != 6)
		return -1;
	return Connect(p1 * 256 + p2);
}

static std::string ReadAll(int s)
{
	std::string data;
	char buffer[4096];
	int n;
	while ((n = recv(s, buffer, sizeof(buffer), 0)) > 0)
		data.append(buffer, n);
	close(s);
	return data;
}

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

int main()
{
	signal(SIGPIPE, SIG_IGN);
	const char *root = getenv("UNLEASHX_ROOT");
	std::string base = std::string(root != NULL ? root : ".") + "/E/ftp";

	// what the network thread would have done
	g_bHostThreads = TRUE;
	g_bNetworkStarted = TRUE;
	g_dwXnAddrStatus = XNET_GET_XNADDR_STATIC | XNET_GET_XNADDR_ETHERNET;
	g_dwIPAddress = htonl(INADDR_LOOPBACK);
	CAppSettings::GetInstance()->m_network.m_bAutoDetect = FALSE;
	int port = 21000 + getpid() % 1000;
	g_ftpServer.SetPort(port);
	CHECK(g_ftpServer.Start());

	int control = Connect(port);
	CHECK(control >= 0);
	if (control < 0)
		return 1;
	std::string reply = ReadReply(control);
	printf("< %s\n", reply.c_str());
	CHECK(reply == "220 UnleashX FTP Server ready.");

	CHECK(StartsWith(Command(control, "USER xbox"), "331 Password required for xbox"));
	CHECK(Command(control, "PASS xbox") == "230 Welcome to XBOX FTP Server");
	CHECK(Command(control, "SYST") == "215 UNIX emulated by [UnleashX] Avalaunch 0.47.5");
	CHECK(Command(control, "PWD") == "257 \"/\" is current directory.");
	CHECK(Command(control, "CWD /E/ftp") == "250 CWD command successful.");
	CHECK(Command(control, "PWD") == "257 \"/E/ftp\" is current directory.");
	CHECK(Command(control, "CWD nowhere") == "553 \"nowhere\": Directory not found.");
	CHECK(Command(control, "TYPE I") == "200 Type set to IMAGE.");
	CHECK(Command(control, "NOOP") == "200 OK.");
	CHECK(Command(control, "XYZZY") == "502 \"XYZZY\" not implemented - Try HELP.");

	// LIST
	int data = OpenPassive(control);
	CHECK(data >= 0);
	CHECK(Command(control, "LIST") == "150 Opening ASCII mode data connection for list.");
	std::string listing = ReadAll(data);
	printf("%s", listing.c_str());
	CHECK(listing.find("..\r\n") != std::string::npos);
	CHECK(listing.find("-rw-r--r--   1 XBOX      XBOX        12 ") != std::string::npos);
	CHECK(listing.find(" hello.txt\r\n") != std::string::npos);
	CHECK(ReadReply(control) == "226 Transfer complete.");

	// RETR
	data = OpenPassive(control);
	CHECK(Command(control, "RETR hello.txt") == "150 Opening BINARY mode data connection for file transfer.");
	CHECK(ReadAll(data) == "Hello, Xbox!");
	CHECK(ReadReply(control) == "226 Transfer complete.");

	// STOR
	std::string upload;
	for (int i = 0; i < 100000; i++)
		upload += (char)('a' + i % 26);
	data = OpenPassive(control);
	CHECK(Command(control, "STOR up.txt") == "150 Opening BINARY mode data connection for file transfer.");
	send(data, upload.c_str(), upload.size(), 0);
	close(data);
	CHECK(ReadReply(control) == "226 Transfer Complete.");
	CHECK(ReadHostFile(base + "/up.txt") == upload);
	CHECK(Command(control, "SIZE up.txt") == "213 100000");

	// folders and renames
	CHECK(Command(control, "MKD sub") == "250 Directory created successfully.");
	CHECK(Command(control, "MKD sub") == "250 Directory already exists.");
	CHECK(Command(control, "RMD sub") == "250 Directory deleted successfully.");
	CHECK(Command(control, "RNFR up.txt") == "350 File exists, ready for destination name.");
	CHECK(Command(control, "RNTO moved.txt") == "250 Rename was successful.");
	CHECK(ReadHostFile(base + "/moved.txt") == upload);
	CHECK(Command(control, "DELE moved.txt") == "250 File \"moved.txt\" was deleted successfully");
	CHECK(ReadHostFile(base + "/moved.txt") == "(missing)");
	CHECK(Command(control, "CDUP") == "250 \"/E\" is current directory.");
	CHECK(Command(control, "QUIT") == "221 Bye");
	close(control);

	Sleep(500);
	g_ftpServer.Stop();
	Sleep(2000);   // let the server's threads finish
	printf(s_failures == 0 ? "ftp_test passed\n" : "ftp_test: %d failures\n", s_failures);
	fflush(stdout);
	_exit(s_failures == 0 ? 0 : 1);
}
