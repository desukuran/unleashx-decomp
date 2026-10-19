// Runs the decompiled HTTP client (CInternetSession, HttpGetText) against a
// small HTTP server on localhost: a text reply kept in memory, a binary one
// saved to the web cache, a redirect, a 404 and the cache itself. Files live
// under $UNLEASHX_ROOT (drive letters are folders).
#include <arpa/inet.h>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <pthread.h>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

#include "../src/common/FileUtil.h"
#include "../src/common/XboxKernel.h"
#include "../src/net/Http.h"
#include "../src/net/Socket.h"

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

static int s_listener = -1;
static int s_port;
static int s_requests;           // how many the server answered
static std::string s_lastRequest;

static void Reply(int s, const std::string &text)
{
	send(s, text.data(), text.size(), 0);
}

// Answers requests one connection at a time until the listener closes.
static void *ServerThread(void *)
{
	for (;;)
	{
		int s = accept(s_listener, NULL, NULL);
		if (s < 0)
			return NULL;
		std::string request;
		char buffer[1024];
		while (request.find("\r\n\r\n") == std::string::npos)
		{
			int n = recv(s, buffer, sizeof(buffer), 0);
			if (n <= 0)
				break;
			request.append(buffer, n);
		}
		s_lastRequest = request;
		s_requests++;
		std::string path = request.substr(request.find(' ') + 1);
		path = path.substr(0, path.find(' '));
		if (path == "/hello.txt")
		{
			Reply(s, "HTTP/1.0 200 OK\r\nContent-Type: text/plain\r\nContent-Length: 12\r\n"
			         "Set-Cookie: id=42; path=/\r\n\r\nHello, Xbox!");
		}
		else if (path == "/data.bin")
		{
			std::string body(3000, 'x');
			Reply(s, "HTTP/1.0 200 OK\r\nContent-Type: application/octet-stream\r\nContent-Length: 3000\r\n\r\n" + body);
		}
		else if (path == "/moved")
		{
			Reply(s, "HTTP/1.0 302 Found\r\nLocation: /hello.txt\r\n\r\n");
		}
		else
		{
			Reply(s, "HTTP/1.0 404 Not Found\r\nContent-Type: text/html\r\n\r\nnope");
		}
		shutdown(s, SHUT_WR);
		while (recv(s, buffer, sizeof(buffer), 0) > 0)
		{
		}
		close(s);
	}
}

static void Wait(CInternetSession &session)
{
	for (int i = 0; i < 200 && !session.IsDone(); i++)
		Sleep(50);
	if (getenv("HTTP_TEST_VERBOSE"))
		printf("state %d status %d requests %d\n", session.m_nState, session.m_nStatus, s_requests);
}

int main()
{
	signal(SIGPIPE, SIG_IGN);
	g_bHostThreads = TRUE;
	g_bHostLinkUp = TRUE;

	s_listener = socket(AF_INET, SOCK_STREAM, 0);
	int yes = 1;
	setsockopt(s_listener, SOL_SOCKET, SO_REUSEADDR, (const char *)&yes, sizeof(yes));
	sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	addr.sin_port = 0;
	if (bind(s_listener, (sockaddr *)&addr, sizeof(addr)) != 0 || listen(s_listener, 4) != 0)
	{
		perror("listen");
		return 1;
	}
	socklen_t length = sizeof(addr);
	getsockname(s_listener, (sockaddr *)&addr, &length);
	s_port = ntohs(addr.sin_port);
	pthread_t server;
	pthread_create(&server, NULL, ServerThread, NULL);

	char url[128];

	// URL parsing.
	SUrl parsed;
	CHECK(ParseUrl("http://www.example.com:8080/a b/c.php?x=1 2", &parsed) == 15);
	CHECK(parsed.port == 8080);
	CHECK(strcmp(parsed.scheme, "http://") == 0);
	CHECK(strcmp(parsed.host, "www.example.com") == 0);
	CHECK(strcmp(parsed.path, "/a%20b/c.php?x=1+2") == 0);
	CHECK(ParseUrl("ftp://host", &parsed) == 4 && parsed.port == 21 && strcmp(parsed.path, "/") == 0);
	CHECK(UrlEncode("a b=c&d", FALSE) == "a+b=c&d");
	CHECK(UrlEncode("x=y", TRUE) == "x%3dy");
	CHECK(UrlEncode("Z[_!", FALSE) == "Z[_%21");   // the 'b' quirk

	// Text stays in memory, and the cookie is kept.
	{
		CInternetSession session;
		snprintf(url, sizeof(url), "http://127.0.0.1:%d/hello.txt", s_port);
		CHECK(session.Get(url, FALSE));
		Wait(session);
		CHECK(session.m_nStatus == 200);
		CHECK(!session.m_bSavedToFile);
		CHECK(strcmp(session.GetData(), "Hello, Xbox!") == 0);
		CHECK(session.m_strCookie == "id=42");
		CHECK(session.m_strContentType == "text/plain");
		CHECK(s_lastRequest.find("GET /hello.txt HTTP/1.0\r\n") == 0);
		CHECK(s_lastRequest.find("User-Agent: Mozilla/4.0") != std::string::npos);

		// A second request on the same session sends the cookie and referer.
		CHECK(session.Get(url, FALSE));
		Wait(session);
		CHECK(s_lastRequest.find("Cookie: id=42\r\n") != std::string::npos);
		CHECK(s_lastRequest.find("Referer: http://127.0.0.1") != std::string::npos);
	}

	// Anything else goes to Y:\WebCache\, and comes from there next time.
	{
		CInternetSession session;
		snprintf(url, sizeof(url), "http://127.0.0.1:%d/data.bin", s_port);
		CHECK(session.Get(url, TRUE));
		Wait(session);
		CHECK(session.m_nStatus == 200);
		CHECK(session.m_bSavedToFile);
		CHECK(session.m_dwDataSize == 3000);
		CHECK(session.m_dwReceived == 3000);
		CHECK(strcmp(session.m_szFile, "Y:\\WebCache\\127.0.0.1data.bin") == 0);
		CHECK(FileExists(session.m_szFile));
		int before = s_requests;
		CHECK(session.Get(url, TRUE));
		Wait(session);
		CHECK(session.m_nStatus == 200 && session.m_bSavedToFile && session.m_dwDataSize == 3000);
		CHECK(s_requests == before);
	}

	// Redirects are followed by IsDone().
	{
		CInternetSession session;
		snprintf(url, sizeof(url), "http://127.0.0.1:%d/moved", s_port);
		CHECK(session.Get(url, FALSE));
		Wait(session);
		CHECK(session.m_nStatus == 200);
		CHECK(strcmp(session.GetData(), "Hello, Xbox!") == 0);
	}

	// Errors.
	{
		CInternetSession session;
		snprintf(url, sizeof(url), "http://127.0.0.1:%d/missing", s_port);
		CHECK(session.Get(url, FALSE));
		Wait(session);
		CHECK(session.m_nStatus == 404);
	}

	// HttpGetText.
	{
		char host[32];
		snprintf(host, sizeof(host), "127.0.0.1");
		std::string text = HttpGetText(host, "/hello.txt", s_port);
		CHECK(text == "Hello, Xbox!");
	}

	shutdown(s_listener, SHUT_RDWR);
	close(s_listener);
	pthread_join(server, NULL);
	if (s_failures == 0)
		printf("http_test: ok\n");
	return s_failures == 0 ? 0 : 1;
}
