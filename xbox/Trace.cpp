// Debug trace for emulator runs: TraceLog appends to a RAM buffer that the
// host reads over QMP (memsave at the map address of g_traceLog).
#include <xtl.h>
#include <stdarg.h>
#include <stdio.h>

extern "C" char g_traceLog[0x10000] = "TRACE\n";
static int s_traceLength = 6;

extern "C" void TraceLog(const char *format, ...)
{
	va_list args;
	va_start(args, format);
	int room = (int)sizeof(g_traceLog) - s_traceLength - 1;
	if (room > 0)
	{
		int n = _vsnprintf(g_traceLog + s_traceLength, room, format, args);
		s_traceLength += n < 0 ? room : n;
	}
	va_end(args);
}
