#include "toyserver.h"
#include <stdarg.h>

const char *ncLog_FileName[] = {
		"ncToyServerSocket.log",
		"ncToyServerClient.log",
		"ncToyServerGame.log",
		"ncToyServerChat.log",
};

void ncLogWrite(int logType, char *msg)
{
	if (logType >= 0 && logType < 4)
	{
		FILE *fp = fopen(ncLog_FileName[logType], "a+");
		if (fp != NULL)
		{
			char date [16];
			_strdate(date);
			const char *time = ncGetTimeString(0);
			fprintf(fp, "%s - %s > ", date, time);
			size_t len = strlen(msg);
			fwrite(msg, 1, len, fp);
			fwrite("\r\n", 1, 2, fp);
			fclose(fp);
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
