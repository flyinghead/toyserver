#include "toyserver.h"
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/select.h>

#include "globals.h"

static ChatInfo tmpInfoChat;

static Stats TotalStats;
static Stats DailyStats;

struct HighScores {
	int times[8];
	char RacerNames[8][16];
	uint8_t RacerCars[8];
	char RacerCountries[8][10];
};
typedef struct HighScores HighScores;
static HighScores TrHighScores;

struct BestLap {
    int time;
    char name[16];
};
typedef struct BestLap BestLap;
static BestLap TrBestLaps[14][8];

static time_t FirstTimer;
static int ToyServerFPS;
static int MinFps = 99999999;
static int MaxFps;
static int64_t NbFrames;

static uint16_t Tcp_Port = 2048;
static uint16_t Udp_Port = 2049;
static int ClientUdpTimeout = 7000;
static int UdpTimeout = 5000;
static short NbCommandPaquetsPerSec = 8;
static short NbDynamixPaquetsPerSec = 6;

static int ProgramTerminated;
static int LastDailyDay;
static int Nb;

void ManageTime(void)
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	TimerRef = tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

void RefreshInfo(int roomId, int verbose)
{
	int nchat = ncServerGetNbInfoChats();
	ChatInfo *chatInfo;
	for (int i = 0; i < nchat; i++)
	{
		chatInfo = ncServerGetInfoChatIndexPtr(i);
		if (chatInfo != NULL && chatInfo->roomId == roomId)
			break;
		chatInfo = NULL;
	}
	if (chatInfo == NULL) {
		if (verbose)
			printf("Info room ID=%d not found !!!\n", roomId);
	}
	else
	{
		nchat = CheckInfoChatFile(chatInfo->highScoreFileName);
		if (nchat != 0)
		{
			chatInfo->msgCount = tmpInfoChat.msgCount;
			memcpy(chatInfo->messages, tmpInfoChat.messages, sizeof(chatInfo->messages));
			if (verbose) {
				printf("Info room ID=%d refreshed :\n", roomId);
				DisplayInfoInfo(roomId);
			}
		}
	}
}

void ConvertTime(unsigned time_ms, unsigned *mins, unsigned *secs, unsigned *hundreths)
{
	unsigned totalSecs = (time_ms / 10) / 100;
	*mins = totalSecs / 60;
	*secs = totalSecs % 60;
	*hundreths = (time_ms / 10) % 100;
}

void ReadHighScoreFile(void)
{
	memset(&TrHighScores, 0, sizeof(TrHighScores));
	FILE *file = fopen("ToyServer.hs", "r");
	if (file != NULL) {
		fread(&TrHighScores, sizeof(TrHighScores), 1, file);
		fclose(file);
	}
	file = fopen("ToyServer.lap", "r");
	if (file != NULL) {
		fread(&TrBestLaps, sizeof(TrBestLaps), 1, file);
		fclose(file);
	}
}

static void HighScoresChatFile(FILE *fp, HighScores *data)
{
	unsigned hundredths;
	unsigned secs;
	unsigned mins;
	fprintf(fp,"NORMAL MODE\n");
	int raceIdx;
	for (raceIdx = 0; raceIdx < 4; raceIdx++)
	{
		if (data->times[raceIdx] == 0) {
			fprintf(fp,"Race %d : --:--:--\n", raceIdx + 1);
		}
		else {
			ConvertTime(data->times[raceIdx], &mins, &secs, &hundredths);
			fprintf(fp, "Race %d : %d:%02d:%02d by \'%s\' on %s\n", raceIdx + 1, mins, secs, hundredths,
					data->RacerNames[raceIdx], TrCars[data->RacerCars[raceIdx]]);
		}
	}
	fprintf(fp," \nREVERSE MODE\n");
	for (; raceIdx < 8; raceIdx++)
	{
		if (data->times[raceIdx] == 0) {
			fprintf(fp, "Race %d : --:--:--\n", raceIdx - 4 + 1);
		}
		else {
			ConvertTime(data->times[raceIdx], &mins, &secs, &hundredths);
			fprintf(fp, "Race %d : %d:%02d:%02d by \'%s\' on %s\n", raceIdx - 4 + 1, mins, secs, hundredths,
					data->RacerNames[raceIdx], TrCars[data->RacerCars[raceIdx]]);
		}
	}
}

void SaveHighScoreFile(int refreshInfo)
{
	uint32_t hundredths;
	uint32_t secs;
	uint32_t mins;

	FILE *fp = fopen("ToyServer.hs", "w");
	if (fp != NULL) {
		fwrite(&TrHighScores, sizeof(TrHighScores), 1, fp);
		fclose(fp);
	}
	fp = fopen("ToyServer.lap", "w");
	if (fp != NULL) {
		fwrite(TrBestLaps, sizeof(TrBestLaps), 1, fp);
		fclose(fp);
	}
	fp = fopen("ToyHighScores.txt", "w");
	if (fp != NULL)
	{
		for (int raceIdx = 0; raceIdx < 8; raceIdx++)
		{
			ConvertTime(TrHighScores.times[raceIdx], &mins, &secs, &hundredths);
			fprintf(fp,"RACE %d : %d:%02d:%02d on %s : \'%s\' (%s)\n", raceIdx + 1, mins, secs, hundredths,
					TrCars[TrHighScores.RacerCars[raceIdx]], TrHighScores.RacerNames[raceIdx], TrHighScores.RacerCountries[raceIdx]);
		}
		fclose(fp);
	}
	fp = fopen("ToyHighScores.cht", "w");
	if (fp != NULL)
	{
		fprintf(fp,"OFFLINE ---------------------------------------------\n \n");
		HighScoresChatFile(fp, &TrHighScores);
		HighScores highScores;
		memset(&highScores, 0, sizeof(highScores));
		for (int raceIdx = 0; raceIdx < 8; raceIdx++)
		{
			for (int i = 0; i < 14; i++)
			{
				if (TrBestLaps[raceIdx][i].time != 0
						&& (highScores.times[raceIdx] == 0 || TrBestLaps[raceIdx][i].time < highScores.times[raceIdx]))
				{
					highScores.times[raceIdx] = TrBestLaps[raceIdx][i].time;
					highScores.RacerCars[raceIdx] = i * 2;
					strcpy(highScores.RacerNames[raceIdx], TrBestLaps[raceIdx][i].name);
				}
			}
		}
		fprintf(fp," \nONLINE ----------------------------------------------\n \n");
		HighScoresChatFile(fp, &highScores);
		fclose(fp);
		if (refreshInfo != 0)
			RefreshInfo(1, 0);
	}
	fp = fopen("ToyBestLaps.txt", "w");
	if (fp != NULL)
	{
		for (int i = 0; i < 14; i++)
		{
			fprintf(fp, "\n%s ----------------------------------------------\n", TrCars[i * 2]);
			for (int raceIdx = 0; raceIdx < 8; raceIdx++)
			{
				int track = raceIdx;
				if (raceIdx == 0)
					fprintf(fp,"NORMAL MODE\n");
				else if (raceIdx == 4)
					fprintf(fp,"REVERSE MODE\n");
				if (raceIdx > 3)
					track -= 4;
				ConvertTime(TrBestLaps[raceIdx][i].time, &mins, &secs, &hundredths);
				fprintf(fp, "RACE %d : %d:%02d:%02d : \'%s\'\n", track + 1, mins, secs, hundredths, TrBestLaps[raceIdx][i].name);
			}
		}
		fclose(fp);
	}
}

void ReadClientInfosMessage(NetMsgT7 *msg)
{
	char save = 0;
	for (int i = 0; i < 8; i++)
	{
		if (msg->highScores[i] != 0 &&
				(TrHighScores.times[i] == 0 || msg->highScores[i] < TrHighScores.times[i]))
		{
			save = 1;
			TrHighScores.times[i] = msg->highScores[i];
			TrHighScores.RacerCars[i] = msg->racerCars[i];
			memcpy(TrHighScores.RacerNames[i], msg->racerNames[i], sizeof(TrHighScores.RacerNames[i]));
			/* FIXME index was not used for country? suspicious packet format */
			memcpy(TrHighScores.RacerCountries[i], msg->racerCountries[i], sizeof(TrHighScores.RacerCountries[i]));
		}
	}
	if (save)
		SaveHighScoreFile(1);
}

void ResetDailyStats(void)
{
	time_t now;
	time(&now);
	struct tm *localTime;
	localTime = localtime(&now);
	LastDailyDay = localTime->tm_mday;
	memset(&DailyStats, 0, sizeof(DailyStats));
}

void SaveDailyStats(void)
{
	FILE *fp = fopen("ToyServer.sta","a+");
	if (fp != NULL)
	{
		char maxConnTime [32];
		char maxGameTime [32];
		const char *timestr = ncGetTimeString(DailyStats.maxConnectionsTime);
		strcpy(maxConnTime, timestr);
		timestr = ncGetTimeString(DailyStats.maxGamesTimes);
		strcpy(maxGameTime, timestr);
		time_t now;
		time(&now);
		now -= 3600;
		struct tm *tm = localtime(&now);
		timestr = ncGetTimeString(DailyStats.maxChatsTime);
		char buf[256];
		sprintf(buf, "%02d/%02d/%d;%d;%d;%d;%d;%d;%d;%d;%s;%d;%s;%d;%s\r\n", tm->tm_mon + 1,
				tm->tm_mday, tm->tm_year + 1900, DailyStats.totalConnections, DailyStats.totalGames,
				DailyStats.totalChats, (uint32_t)DailyStats.connectionTime, (uint32_t)DailyStats.gameTime, (uint32_t)DailyStats.chatTime,
				DailyStats.maxConnections, maxConnTime, DailyStats.maxGames, maxGameTime,
				DailyStats.maxChats, timestr);
		size_t len = strlen(buf);
		fwrite(buf, 1, len, fp);
		fclose(fp);
	}
	ResetDailyStats();
}

int ncServerGetClientUdpTimeout(void) {
	return ClientUdpTimeout;
}

void ncServerGetClientTimers(short *dynPackPerSec,short *cmdPackPerSec)
{
	if (dynPackPerSec != NULL)
		*dynPackPerSec = NbDynamixPaquetsPerSec;
	if (cmdPackPerSec != NULL)
		*cmdPackPerSec = NbCommandPaquetsPerSec;
}

void NewInfoChatRoom(void) {
}

int ReadCfgInt(char *start, char *end, char **value)
{
	char *p = start;
	while (p < end && *p > ' ')
		p++;
	*p = '\0';
	*value = p + 1;
	return atoi(start);
}

int ReadCfgInfoChat(char *start, char *end, char **value)
{
	char *nextLine = strtok(start, "\n\r");
	if (nextLine != NULL)
		*value = strtok(NULL, "");
	char *token = strtok(start, ",");
	if (token == NULL)
		return 0;
	strcpy(tmpInfoChat.chatName, token);
	token = strtok(NULL, ",");
	if (token == NULL)
		return 0;
	for (int i = 0; i < 5; i++)
	{
		if (strcasecmp(token, LangNames[i]) == 0) {
			tmpInfoChat.language = i;
			break;
		}
		if (i == 4)
			return 0;
	}
	token = strtok(NULL, ",");
	if (token == NULL)
		return 0;
	strcpy(tmpInfoChat.highScoreFileName, token);
	if (CheckInfoChatFile(tmpInfoChat.highScoreFileName) == 0)
		return 0;
	if (ncServerCreateInfoChat(&tmpInfoChat) == 0)
		return 0;
	NewInfoChatRoom();
	return 1;
}

int CreateInfoHighScores(void)
{
	FILE *fp = fopen("ToyHighScores.cht","r");
	if (fp == NULL)
		SaveHighScoreFile(0);
	else
		fclose(fp);
	strcpy(tmpInfoChat.chatName, "High Scores");
	strcpy(tmpInfoChat.highScoreFileName, "ToyHighScores.cht");
	tmpInfoChat.language = 0;
	if (CheckInfoChatFile("ToyHigh" "Scores.cht") == 0)
		return 0;
	if (ncServerCreateInfoChat(&tmpInfoChat) == 0)
		return 0;
	NewInfoChatRoom();
	return 1;
}

int ReadCfgFile(char *filename)
{
	FILE *fp = fopen(filename, "r");
	if (fp == NULL)
		return 0;
	fseek(fp, 0, SEEK_END);
	size_t fileSize = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	char *fileStart = (char *)malloc(fileSize + 1);
	if (fileStart == NULL) {
		fclose(fp);
		return 0;
	}
	fileStart[fileSize] = '\0';
	size_t read = fread(fileStart, 1, fileSize, fp);
	fclose(fp);
	if (read == 0) {
		free(fileStart);
		return 0;
	}
	int rc = 0;
	char *optName = fileStart;
	while (optName < fileStart + fileSize)
	{
		char *value;
		for (value = optName; value < fileStart + fileSize && *value != ':'; value++) {
		}
		if (*value != ':')
			break;
		*value = '\0';
		if (strcasecmp(optName, "TCP_Port") == 0)
		{
			Tcp_Port = (uint16_t)ReadCfgInt(value + 1, fileStart + fileSize, &optName);
			if (Tcp_Port == 0)
				goto EXIT;
		}
		else if (strcasecmp(optName, "UDP_Port") == 0)
		{
				Udp_Port = (uint16_t)ReadCfgInt(value + 1,fileStart + fileSize,&optName);
				if (Udp_Port == 0)
					goto EXIT;
		}
		else if (strcasecmp(optName, "NbPlayers") == 0)
		{
			MaxNbPlayers = ReadCfgInt(value + 1, fileStart + fileSize, &optName);
			if (MaxNbPlayers == 0)
				goto EXIT;
		}
		else if (strcasecmp(optName, "NbGames") == 0)
		{
			MaxNbGames = ReadCfgInt(value + 1, fileStart + fileSize, &optName);
			if (MaxNbGames == 0)
				goto EXIT;
		}
		else if (strcasecmp(optName, "NbChats") == 0)
		{
			MaxNbChats = ReadCfgInt(value + 1, fileStart + fileSize, &optName);
			if (MaxNbChats == 0)
				goto EXIT;
			if (ncChatRoomAllocate(MaxNbChats) == 0)
				goto EXIT;
			if (CreateInfoHighScores() == 0)
				goto EXIT;
		}
		else if (strcasecmp(optName, "Command_paquets_per_sec") == 0)
		{
			NbCommandPaquetsPerSec = (short)ReadCfgInt(value + 1, fileStart + fileSize, &optName);
			if (NbCommandPaquetsPerSec == 0)
				goto EXIT;
		}
		else if (strcasecmp(optName, "Dynamix_paquets_per_sec") == 0)
		{
			NbDynamixPaquetsPerSec = (short)ReadCfgInt(value + 1, fileStart + fileSize, &optName);
			if (NbDynamixPaquetsPerSec == 0)
				goto EXIT;
		}
		else if (strcasecmp(optName, "client_UDP_time_out") == 0) {
			ClientUdpTimeout = ReadCfgInt(value + 1, fileStart + fileSize, &optName);
		}
		else if (strcasecmp(optName, "UDP_time_out") == 0) {
			UdpTimeout = ReadCfgInt(value + 1, fileStart + fileSize, &optName);
		}
		else
		{
			static int nbInfoChat;
			if (strcasecmp(optName, "info_Chat_list_nb") == 0) {
				nbInfoChat = ReadCfgInt(value + 1, fileStart + fileSize, &optName);
			}
			else if (strcasecmp(optName, "info_Chat_list_start") == 0)
			{
				do {
					optName = value + 1;
					if (fileStart + fileSize <= optName)
						break;
					value = optName;
				} while (*optName <= ' ');
				int i = 0;
				while (i < nbInfoChat)
				{
					if (ReadCfgInfoChat(optName, fileStart + fileSize, &optName) == 0)
						goto EXIT;
					i++;
				}
			}
			else {
				//if (strcasecmp(optName,"info_Chat_list_end") == 0)
				optName = value + 1;
			}
		}
	}
	rc = 1;
EXIT:
	free(fileStart);
	if (rc == 0)
		printf("ERROR : Invalid config file: %s\n", filename);

	return rc;
}

void NewClientCallback(void)
{
	TotalStats.totalConnections = TotalStats.totalConnections + 1;
	TotalStats.connectionCount = TotalStats.connectionCount + 1;
	if (TotalStats.maxConnections < TotalStats.connectionCount) {
		TotalStats.maxConnections = TotalStats.connectionCount;
		time(&TotalStats.maxConnectionsTime);
	}
	DailyStats.totalConnections = DailyStats.totalConnections + 1;
	DailyStats.connectionCount = DailyStats.connectionCount + 1;
	if (DailyStats.maxConnections < DailyStats.connectionCount) {
		DailyStats.maxConnections = DailyStats.connectionCount;
		time(&DailyStats.maxConnectionsTime);
	}
}

void DisconnectClientCallback(Client *client)
{
	time_t now;
	time(&now);
	time_t duration = now - client->connectTime;
	TotalStats.connectionTime = TotalStats.connectionTime + duration;
	TotalStats.connectionCount = TotalStats.connectionCount - 1;
	DailyStats.connectionTime = DailyStats.connectionTime + duration;
	DailyStats.connectionCount = DailyStats.connectionCount - 1;
}

void NewChatRoomCallback(void)
{
	TotalStats.totalChats = TotalStats.totalChats + 1;
	TotalStats.chatCount = TotalStats.chatCount + 1;
	if (TotalStats.maxChats < TotalStats.chatCount) {
		TotalStats.maxChats = TotalStats.chatCount;
		time(&TotalStats.maxChatsTime);
	}
	DailyStats.totalChats = DailyStats.totalChats + 1;
	DailyStats.chatCount = DailyStats.chatCount + 1;
	if (DailyStats.maxChats < DailyStats.chatCount) {
		DailyStats.maxChats = DailyStats.chatCount;
		time(&DailyStats.maxChatsTime);
	}
}

void DeleteChatRoomCallback(ChatRoom *chatRoom)
{
	time_t now;
	time(&now);
	time_t duration = now - chatRoom->creationTime;
	TotalStats.chatTime = TotalStats.chatTime + duration;
	TotalStats.chatCount = TotalStats.chatCount - 1;
	DailyStats.chatTime = DailyStats.chatTime + duration;
	DailyStats.chatCount = DailyStats.chatCount - 1;
}

void NewGameCallback(void)
{
	TotalStats.totalGames = TotalStats.totalGames + 1;
	TotalStats.gameCount = TotalStats.gameCount + 1;
	if (TotalStats.maxGames < TotalStats.gameCount) {
		TotalStats.maxGames = TotalStats.gameCount;
		time(&TotalStats.maxGamesTimes);
	}
	DailyStats.totalGames = DailyStats.totalGames + 1;
	DailyStats.gameCount = DailyStats.gameCount + 1;
	if (DailyStats.maxGames < DailyStats.gameCount) {
		DailyStats.maxGames = DailyStats.gameCount;
		time(&DailyStats.maxGamesTimes);
	}
}

void DeleteGameCallback(GameRoom *gameRoom)
{
	time_t now;
	time(&now);
	time_t duration = now - gameRoom->creationTime;
	TotalStats.gameTime = TotalStats.gameTime + duration;
	TotalStats.gameCount = TotalStats.gameCount - 1;
	DailyStats.gameTime = DailyStats.gameTime + duration;
	DailyStats.gameCount = DailyStats.gameCount - 1;
}

void ResultCallback(GameRoom *gameRoom)
{
	uint8_t ranking[4];
	int newBestLap = 0;

	uint8_t trackId = gameRoom->gameParams[3] & 0x7f;
	if (trackId > 3)
		trackId = 0;
	if (((gameRoom->gameParams[1] >> 3) & 1) != 0)
		trackId = trackId + 4;
	for (int playerNum = 0; playerNum < (int)gameRoom->playerCount; playerNum++)
	{
		Result *result = &gameRoom->results[playerNum];
		int carId = gameRoom->playerParams[playerNum] >> 1;
		if (result->bestLap != 0
				&& (TrBestLaps[trackId][carId].time == 0 || result->bestLap < TrBestLaps[trackId][carId].time))
		{
			TrBestLaps[trackId][carId].time = result->bestLap;
			strcpy(TrBestLaps[trackId][carId].name, gameRoom->players[playerNum]->listItem.name);
			NetMsgT14 msg;
			memset(&msg, 0, sizeof(msg));
			msg.head.msgType = 14;
			msg.head.msgId = 0;
			unsigned hundredths;
			unsigned secs;
			unsigned mins;
			ConvertTime(result->bestLap, &mins, &secs, &hundredths);
			sprintf(msg.message, "Congratulation %s! New BEST LAP %d:%02d:%02d on race %d with %s",
					gameRoom->players[playerNum]->listItem.name, mins, secs, hundredths, trackId + 1,
					TrCars[carId * 2]);
			uint16_t len = strlen(msg.message);
			msg.head.size = (len + 16) & 0xfffc;
			ncGameSendMsgExceptFrom(gameRoom, &msg.head, 0);
			newBestLap = 1;
		}
		int totalSecs = result->totalSecs;
		float local_34;
		float local_30;
		memcpy(&local_30,&result->unk2,4);	// FIXME floats
		memcpy(&local_34,&result->unk1,4);
		local_30 = local_30 + local_34;
		ranking[playerNum] = 1;
		for (int idx = 0; idx < (int)gameRoom->playerCount; idx++)
		{
			if (idx == playerNum)
				continue;
			Result *result = &gameRoom->results[idx];
			int secs = result->totalSecs;
			if (totalSecs == 0)
			{
				float local_15c;
				float local_158;
				memcpy(&local_158,&result->unk2,4);	// FIXME floats
				memcpy(&local_15c,&result->unk1,4);
				local_158 = local_158 + local_15c;
				if (local_30 < local_158 || secs != 0)
					ranking[playerNum] += 1;
			}
			else if (secs != 0 && secs < totalSecs) {
				ranking[playerNum] += 1;
			}
		}
		if (ranking[playerNum] == 1) {
			Client *client = gameRoom->players[playerNum];
			client->racesWon += 1;
		}
	}
	uint32_t tmp;
	memcpy(&tmp, ranking, sizeof(ranking));
	for (int i = 0; i < (int)gameRoom->playerCount; i++)
		ncServerSendClientTcpStandardMsg(gameRoom->players[i], 39, tmp);
	for (int i = 0; i < (int)gameRoom->playerCount; i++)
		gameRoom->pointsWon[i] = gameRoom->pointsWon[i] + 4 - ranking[i];
	memcpy(&tmp, gameRoom->pointsWon, sizeof(gameRoom->pointsWon));
	for (int i = 0; i < (int)gameRoom->playerCount; i++)
		ncServerSendClientTcpStandardMsg(gameRoom->players[i], 43, tmp);
	if (newBestLap != 0)
		SaveHighScoreFile(1);
}

int ServerStart(void)
{
	memset(&TotalStats, 0, sizeof(TotalStats));
	ResetDailyStats();
	ncLogWrite(0, "**************************************************************************************");
	ncLogWrite(1, "**************************************************************************************");
	ncLogWrite(2, "**************************************************************************************");
	ncLogWrite(3, "**************************************************************************************");
	ReadHighScoreFile();
	FILE *fp = fopen("ToyServer.cfg", "r");
	fclose(fp);
	if (fp != NULL && ReadCfgFile("ToyServer.cfg") == 0)
		return 0;
	if (fp == NULL) {
		if (ncChatRoomAllocate(MaxNbChats) == 0 || CreateInfoHighScores() == 0)
			return 0;
	}
	if (ncServerClientAllocate(MaxNbPlayers) == 0)
		return 0;
	if (ncGameRoomAllocate(MaxNbGames) == 0)
		return 0;
	if (ncServerClientInit() == 0)
		return 0;
	if (ncServerSocketInit(Tcp_Port,Udp_Port) == 0)
		return 0;
	ncServerSetUdpTimeout(UdpTimeout);
	ncServerSetNewClientCallback(NewClientCallback);
	ncServerSetDisconnectClientCallback(DisconnectClientCallback);
	ncChatRoomSetNewCallback(NewChatRoomCallback);
	ncChatRoomSetDeleteCallback(DeleteChatRoomCallback);
	ncGameSetNewCallback(NewGameCallback);
	ncGameRoomSetDeleteCallback(DeleteGameCallback);
	ncGameSetResultCallback(ResultCallback);
	ncServerClientSetInfosCallback(ReadClientInfosMessage);
	return 1;
}

void ServerManage(void)
{
	static time_t lastrefresh;

	ncChatRoomManage();
	ncGameRoomManage();
	ncServerManageClients();
	if (lastrefresh == 0 || lastrefresh < TimerRef)
	{
		lastrefresh = TimerRef + 1000;
		time_t now;
		time(&now);
		struct tm *tm = localtime(&now);
		if (LastDailyDay != tm->tm_mday)
			SaveDailyStats();
	}
}

void ServerShutDown(void)
{
	SaveDailyStats();
	ncServerSendAllClientsStandardTcpMsg(5, 0);
#ifdef NDEBUG
	sleep(3);
#endif
	ncServerSocketRelease();
	ncChatRoomRelease();
	ncGameRoomRelease();
	ncServerClientRelease();
}

int CheckInfoChatFile(char *fileName)
{
	FILE *fp = fopen(fileName, "r");
	if (fp == NULL) {
		printf("ERROR : Can\'t open file %s.\n", fileName);
		return 0;
	}
	fseek(fp, 0, SEEK_END);
	size_t fileSize = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	if (fileSize >= 25600u)
	{
		fclose(fp);
		printf("ERROR : File %s too big !\n", fileName);
		return 0;
	}
	char buf[25600];
	if (fileSize == 0 || fread(buf, fileSize, 1, fp) != 1)
	{
		fclose(fp);
		printf("ERROR : Can\'t read file %s.\n", fileName);
		return 0;
	}
	fclose(fp);
	tmpInfoChat.msgCount = 0;
	char *end = buf + fileSize;
	char *line = buf;
	while (line < end)
	{
		if (tmpInfoChat.msgCount >= 100) {
			printf("ERROR : Too many lines in text file %s.\n", fileName);
			return 0;
		}
		char *eol = line;
		while (eol < end && *eol >= ' ')
			eol++;
		*eol = '\0';
		if (strlen(line) > 255) {
			printf("ERROR : Text line too long!.\n");
			return 0;
		}
		strcpy(tmpInfoChat.messages[tmpInfoChat.msgCount], line);
		tmpInfoChat.msgCount++;
		while (eol < end && *eol < ' ')
			eol++;
		line = eol;
	}
	return 1;
}

void DisplayStats(void)
{
	const char *timestr = ncGetTimeString(TotalStats.maxConnectionsTime);
	printf("Connections: \tcurr: %d max: %d (at %s) total: %d\n", TotalStats.connectionCount,
			TotalStats.maxConnections, timestr, TotalStats.totalConnections);
	timestr = ncGetTimeString(TotalStats.maxGamesTimes);
	printf("Games: \t\tcurr: %d max: %d (at %s) total: %d\n", TotalStats.gameCount, TotalStats.maxGames,
			timestr, TotalStats.totalGames);
	timestr = ncGetTimeString(TotalStats.maxChatsTime);
	printf("Chats: \t\tcurr: %d max: %d (at %s) total: %d\n", TotalStats.chatCount, TotalStats.maxChats,
			timestr, TotalStats.totalChats);
	printf("\nTCP sent:%d, recv:%d, UDP sent:%d, recv:%d, lost:%d, max UDP size:%d\n\n", ncNbTcpSent,
			ncNbTcpReceived, ncNbUdpSent, ncNbUdpReceived, ncNbUdpLost, MaxUdpMsgSize);
	if (TimerRef > FirstTimer) {
		unsigned avgFps = (unsigned)(NbFrames * 1000 / (TimerRef - FirstTimer));
		printf("FPS : curr=%u min=%u max=%u avg=%u\n\n", ToyServerFPS, MinFps, MaxFps, avgFps);
	}
}

void DisplayHigh(void)
{
	uint32_t hundredths;
	uint32_t secs;
	uint32_t mins;
	int raceIdx;

	for (raceIdx = 0; raceIdx < 8; raceIdx = raceIdx + 1) {
		ConvertTime(TrHighScores.times[raceIdx],&mins,&secs,&hundredths);
		printf("RACE %d : %d:%02d:%02d on %s : \'%s\' (%s)\n",raceIdx + 1,mins,secs,hundredths,
				TrCars[TrHighScores.RacerCars[raceIdx]],TrHighScores.RacerNames[raceIdx],TrHighScores.RacerCountries[raceIdx]);
	}
}

static void breakhandler(int signum) {
	ProgramTerminated = 1;
}

static void GameListCallback(GameRoom *gameRoom, void *arg) {
	Nb = Nb + 1;
	printf("%d> \'%s\' (ID=%d) %d/%d\n", Nb, gameRoom->listItem.name, gameRoom->listItem.id,
			gameRoom->playerCount, gameRoom->maxPlayers);
}

static void ChatListCallback(ChatRoom *chatRoom, void *arg) {
	Nb = Nb + 1;
	printf("%d> \'%s\' (ID=%d) %d client(s)\n", Nb, chatRoom->listItem.name, chatRoom->listItem.id,
			chatRoom->userCount);
}

static void PlayerListCallback(Client *client, void *arg) {
	Nb = Nb + 1;
	printf("%d> \'%s\' ID=%d\n", Nb, client->listItem.name, client->listItem.id);
}

void _strdate(char *date)
{
	time_t now;
	time(&now);
	struct tm *tm = localtime(&now);
	sprintf(date,"%02d/%02d/%04d", tm->tm_mon + 1, tm->tm_mday, tm->tm_year + 1900);
}

void DisplayChatInfo(int roomId)
{
	ChatRoom *chatRoom = ncServerGetChatById(roomId);
	if (chatRoom == NULL) {
		printf("Chat room ID=%d not found !!!\n", roomId);
		return;
	}
	printf("Chat room \'%s\' (ID=%d) language : %s\n", chatRoom->listItem.name,
			chatRoom->listItem.id, Languages[chatRoom->language]);
	char strdate[16];
	_strdate(strdate);
	time_t now;
	time(&now);
	const char *secs = ncConvertSeconds(now - chatRoom->creationTime);
	const char *timestr = ncGetTimeString(chatRoom->creationTime);
	printf("\tCreated %s - %s for %s\n", strdate, timestr, secs);
	printf("\t%d clients :\n", chatRoom->userCount);
	for (int i = 0; i < (int)chatRoom->userCount; i++)
		printf("\t\t- \'%s\' ID=%d\n", chatRoom->clients[i]->listItem.name,
				chatRoom->clients[i]->listItem.id);
}

void DisplayGameInfo(int gameId)
{
	GameRoom *gameRoom = ncServerGetGameById(gameId);
	if (gameRoom == NULL) {
		printf("Game ID=%d not found !!!\n", gameId);
		return;
	}
	printf("Game \'%s\' (ID=%d)\n", gameRoom->listItem.name, gameRoom->listItem.id);
	char dateString[16];
	_strdate(dateString);
	time_t now;
	time(&now);
	const char *seconds = ncConvertSeconds(now - gameRoom->creationTime);
	const char *timeString = ncGetTimeString(gameRoom->creationTime);
	printf("\tCreated %s - %s for %s\n", dateString, timeString, seconds);
	printf("\tStatus : %s\n", GameStatus[gameRoom->status]);
	printf("\t%d players (max %d) :\n", gameRoom->playerCount, gameRoom->maxPlayers);
	printf("\tparams : %u %u %u %u\n", gameRoom->gameParams[0], gameRoom->gameParams[1],
			gameRoom->gameParams[2], gameRoom->gameParams[3]);
	for (int i = 0; i < (int)gameRoom->playerCount; i++)
		printf("\t\t- \'%s\' ID=%d (ready:%d)\n", gameRoom->players[i]->listItem.name,
				gameRoom->players[i]->listItem.id, gameRoom->playerReady[i]);
}

void DisplayPlayerInfo(int clientId)
{
	Client *client = ncClientFindById(clientId);
	if (client == NULL) {
		printf("Player ID=%d not found !!!\n", clientId);
		return;
	}
	printf("Player \'%s\' (ID=%d)\n", client->listItem.name, client->listItem.id);
	char datestring[16];
	_strdate(datestring);
	time_t now;
	time(&now);
	const char *secs = ncConvertSeconds(now - client->connectTime);
	const char *strtime = ncGetTimeString(client->connectTime);
	printf("\tConnected since %s - %s for %s\n", datestring, strtime, secs);
	printf("\tStatus : %s\n", StatusName[client->status]);
}

void ListInfos(void)
{
	int nInfoChat = ncServerGetNbInfoChats();
	for (int i = 0; i < nInfoChat; i = i + 1)
	{
		ChatInfo *chatInfo = ncServerGetInfoChatIndexPtr(i);
		if (chatInfo != NULL)
			printf("%d> \'%s\' (ID=%d)\n", i + 1, chatInfo->chatName, chatInfo->roomId);
	}
	printf("%d info rooms listed.\n", nInfoChat);
}

void DisplayInfoInfo(int roomId)
{
	int n = ncServerGetNbInfoChats();
	ChatInfo *chatInfo = NULL;
	for (int i = 0; i < n; i++)
	{
		chatInfo = ncServerGetInfoChatIndexPtr(i);
		if (chatInfo != NULL && chatInfo->roomId == roomId)
			break;
		chatInfo = NULL;
	}
	if (chatInfo == NULL) {
		printf("Info room ID=%d not found !!!\n",roomId);
		return;
	}
	const char *lang = Languages[chatInfo->language];
	printf("Info room \'%s\' (ID=%d) language : %s\n", chatInfo->chatName, chatInfo->roomId, lang);
	printf("\tText file : %s\n", chatInfo->highScoreFileName);
	printf("\t%d message lines :\n", chatInfo->msgCount);
	for (int i = 0; i < chatInfo->msgCount; i++)
		printf("\t\t- %s\n", chatInfo->messages[i]);
}

void ServerCommand(char *cmd)
{
	static int status;
	static char ServerMsg[256];
	static int Victim;

	switch (status)
	{
	case 0:
		if (strcasecmp(cmd,"quit") == 0)
		{
			printf("\nSHUT DOWN SERVER ???\nAre-you SURE (yes/no) ?\n");
			status = 1;
			return;
		}
		if (strcasecmp(cmd, "msg") == 0) {
			printf("\nEnter your message, it may have several, lines\nEnter #END# as a new line to send the message\nYou\'ll be asked for confirmation\nMessage :\n");
			status = 10;
			ServerMsg[0] = 0;
			return;
		}
		if (strcasecmp(cmd, "list") == 0 || strcasecmp(cmd, "l") == 0) {
			printf("\nList what ???\n(p)layers/(c)hat rooms/(g)ames/(i)nfo rooms ?\n");
			status = 3;
			return;
		}
		if (strcasecmp(cmd, "info") == 0 || strcasecmp(cmd, "i") == 0) {
			printf("\nGet infos on what ???\n(p)layers/(c)hat rooms/(g)ames/(i)nfo rooms ?\n");
			status = 4;
			return;
		}
		if (strcasecmp(cmd, "refresh") == 0) {
			printf("Enter info room\'s ID :\n");
			status = 12;
			return;
		}
		if (strcasecmp(cmd, "kill") == 0) {
			printf("\nKILL Who ???\nEnter player ID :\n");
			status = 9;
			return;
		}
		if (strcasecmp(cmd, "stats") == 0 || strcasecmp(cmd, "s") == 0) {
			DisplayStats();
		}
		else if ((strcasecmp(cmd, "high") == 0) || strcasecmp(cmd, "h") == 0) {
			DisplayHigh();
		}
		else
		{
			if (strcasecmp(cmd,"help") != 0)
				printf("Unknown command !!!\n");
			printf("\tHELP    : some help...\n");
			printf("\tKILL    : kill someone ?\n");
			printf("\tLIST (L): some lists...\n");
			printf("\tINFO (I): some infos...\n");
			printf("\tSTATS(S): some stats...\n");
			printf("\tHIGH (H): high scores...\n");
			printf("\tMSG     : send a message to all players...\n");
			printf("\tREFRESH : reload info room\'s text file...\n");
			printf("\tQUIT    : shut down the server !\n\n");
		}
		printf("\ncommand :->\n");
		break;
	case 1:	// Shutdown
		if (strcasecmp(cmd,"yes") == 0)
			ProgramTerminated = 1;
		else
			printf("SHUT DOWN SERVER canceled\n\n");
		status = 0;
		printf("\ncommand :->\n");
		break;
	case 2:	// Kill
		if (strcasecmp(cmd, "yes") == 0)
		{
			Client *client = ncClientFindById(Victim);
			if (client != NULL) {
				printf("Bye, bye Mr \'%s\'...\nPlayer ID=%d killed.", client->listItem.name, client->listItem.id);
				ncServerDisconnectClient(client);
			}
		}
		else {
			printf("KILL cancelled\n");
		}
		Victim = 0;
		status = 0;
		printf("\ncommand :->\n");
		break;
	case 3:	// List
		if (cmd[1] == '\0')
		{
			switch (*cmd)
			{
			case 'C':
			case 'c':
				Nb = 0;
				ncServerEnumChatRooms(ChatListCallback, NULL);
				printf("%d chat rooms listed.\n", Nb);
				break;
			case 'G':
			case 'g':
				Nb = 0;
				ncServerEnumGameRooms(GameListCallback, NULL);
				printf("%d games listed.\n", Nb);
				break;
			case 'I':
			case 'i':
				ListInfos();
				break;
			case 'P':
			case 'p':
				Nb = 0;
				ncServerEnumClients(PlayerListCallback, NULL);
				printf("%d players listed.\n", Nb);
				break;
			default:
				printf("error\n");
				break;
			}
		}
		status = 0;
		printf("\ncommand :->\n");
		break;
	case 4:	// Info
		if (cmd[1] == '\0')
		{
			switch (*cmd)
			{
			case 'C':
			case 'c':
				status = 7;
				printf("Enter chat\'s ID :\n");
				break;
			case 'G':
			case 'g':
				status = 6;
				printf("Enter game\'s ID :\n");
				break;
			case 'I':
			case 'i':
				status = 8;
				printf("Enter info room\'s ID :\n");
				break;
			case 'P':
			case 'p':
				status = 5;
				printf("Enter player\'s ID :\n");
				break;
			default:
				printf("error\n");
				status = 0;
				printf("\ncommand :->\n");
				break;
			}
		}
		break;
	case 5:
	case 6:
	case 7:
	case 8:
	case 0xc:
		{
			int id = atoi(cmd);
			switch (status)
			{
			case 5:
				DisplayPlayerInfo(id);
				break;
			case 6:
				DisplayGameInfo(id);
				break;
			case 7:
				DisplayChatInfo(id);
				break;
			case 8:
				DisplayInfoInfo(id);
				break;
			case 12:
				RefreshInfo(id, 1);
			}
			status = 0;
			printf("\ncommand :->\n");
			break;
		}
	case 9:
		{
			Victim = atoi(cmd);
			Client *client = ncClientFindById(Victim);
			if (client == NULL)
			{
				printf("Player ID=%d not found !\n", Victim);
				Victim = 0;
				status = 0;
				printf("\ncommand :->\n");
			}
			else {
				printf("Do-you really want to KILL player \'%s\' (ID=%d)?\n(yes/no) ?\n",
						client->listItem.name, client->listItem.id);
				status = 2;
			}
			break;
		}
	case 10:
		if (strcasecmp(cmd,"#end#") == 0) {
			status = 11;
			printf("End of message, OK\nSend this Message to ALL players :\n%s\n(yes/no) ?\n", ServerMsg);
		}
		else {
			if (strlen(ServerMsg) + strlen(cmd) + 2 < 256) {
				strcat(ServerMsg,cmd);
				strcat(ServerMsg,"\n");
			}
			else {
				printf("ERROR : message too long, ABORTED !\n\n");
				status = 0;
				printf("\ncommand :->\n");
			}
		}
		break;
	case 0xb:
		if (strcasecmp(cmd,"yes") == 0)
		{
			NetMsgT14 msg;
			memset(&msg, 0, sizeof(msg));
			msg.head.size = 268;
			msg.head.msgType = 40;
			strcpy(msg.message, ServerMsg);
			ncServerSendAllClientsTcpMsgExceptFrom(&msg.head, 0);
			printf("OK, message sent\n");
		}
		else {
			printf("Message cancelled\n");
		}
		status = 0;
		printf("\ncommand :->\n");
	}
}

static void handleCommand(void *a)
{
	char buf[256];
	int nchars = read(STDIN_FILENO, buf, sizeof(buf));
	if (nchars > 1) {
		buf[nchars - 1] = '\0';
		ServerCommand(buf);
	}
}

int main(int argc,char **argv)
{
	static int64_t next_nb_frames;
	static time_t last_timer;

	struct sigaction sigact;
	memset(&sigact, 0, sizeof(sigact));
	sigact.sa_handler = breakhandler;
	sigact.sa_flags = SA_RESTART;
	sigaction(SIGINT, &sigact, NULL);

	printf("Starting TOY RACER server...\n");
	ManageTime();
	if (FirstTimer == 0)
		FirstTimer = TimerRef;
	if (ServerStart() == 0) {
		printf("Can\'t initialize network !\n\n");
		return 1;
	}
	pollReadSocket(STDIN_FILENO, handleCommand, NULL);

	printf("TOY RACER server (version %d) started !\n\n", SERVER_VERSION);
	printf("\ncommand :->\n");
	while (ProgramTerminated == 0)
	{
		pollWait(TotalStats.connectionCount == 0 ? 1000 : 10);	// wait 1 s when idle, 10 ms when active
		ManageTime();
		NbFrames += 1;
		if (next_nb_frames == 0) {
			next_nb_frames = NbFrames + 1000;
			last_timer = TimerRef;
		}
		else if (next_nb_frames <= NbFrames)
		{
			next_nb_frames = 0;
			if (TimerRef > last_timer)
			{
				ToyServerFPS = (int)(1000000 / (TimerRef - last_timer));
				if (ToyServerFPS < MinFps)
					MinFps = ToyServerFPS;
				if (ToyServerFPS > MaxFps)
					MaxFps = ToyServerFPS;
			}
		}
		ServerManage();
	}
	stopPollingSocket(STDIN_FILENO);
	printf("TOY RACER server shutting down...\n");
	ServerShutDown();
	printf("OK, bye bye !\n");

	return 0;
}




