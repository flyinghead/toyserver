#include "toyserver.h"
#include <stdarg.h>

#ifndef NDEBUG
static const char *ncLog_FileName[] = {
		"ncToyServerSocket.log",
		"ncToyServerClient.log",
		"ncToyServerGame.log",
		"ncToyServerChat.log",
};
#endif

static void ncLogWrite(int logType, char *msg)
{
	if (logType >= 0 && logType < 4)
	{
#ifdef NDEBUG
		FILE *fp = stderr;
		switch (logType)
		{
		case 0:
			fprintf(stderr, "socket: ");
			break;
		case 1:
			fprintf(stderr, "client: ");
			break;
		case 2:
			fprintf(stderr, "game: ");
			break;
		case 3:
			fprintf(stderr, "chat: ");
			break;
		}
#else
		FILE *fp = fopen(ncLog_FileName[logType], "a+");
#endif
		if (fp != NULL)
		{
			char date[32];
			_strdate(date);
			const char *time = ncGetTimeString(0);
			fprintf(fp, "%s - %s > %s\n", date, time, msg);
#ifdef NDEBUG
			fflush(fp);
#else
			fclose(fp);
#endif
		}
	}
}

void ncLogPrintf(int logType, char *format, ...)
{
	static char ncLogBuf[1024];

	va_list ap;
	va_start(ap, format);
	int l = vsprintf(ncLogBuf, format, ap);
	va_end(ap);
	if (l != -1)
		ncLogWrite(logType, ncLogBuf);
}
