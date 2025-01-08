#include "toyserver.h"
#include <time.h>

static List GameRooms;
static int Udp_Timeout = 5000;
static int LastRaceID = 1;

static void (*ncGameRoomNewCallback)(GameRoom *);
static void (*ncGameRoomJoinCallback)(GameRoom *);
static void (*ncServerGameResultCallback)(GameRoom *);
static void (*ncGameRoomQuitCallback)(Client *);

GameRoom *ncServerGetGameById(int gameId) {
	return (GameRoom *)ncListFindById(&GameRooms, gameId);
}

void ncGameStatusChanged(GameRoom *gameRoom)
{
	NetMsgT33 msg;

	memset(&msg, 0, sizeof(msg));
	msg.head.msgId = gameRoom->listItem.id;
	msg.head.size = 16;
	msg.head.msgType = 33;
	msg.status = gameRoom->status;
	ncServerSendAllClientsTcpMsgExceptFrom(&msg.head, 0);
}

void ncServerSetUdpTimeout(int timeout) {
	Udp_Timeout = timeout;
}

int ncGameRoomAllocate(int maxGameRooms) {
	MaxNbGames = maxGameRooms;
	return ncListAllocate(&GameRooms, maxGameRooms, sizeof(GameRoom));
}

void ncGameRoomRelease(void) {
	ncListRelease(&GameRooms);
}

GameRoom *ncGameRoomCreate(Client *client, char *gameName, uint8_t *gameParams, uint8_t maxPlayers)
{
	if (ncListCheckNameUsed(&GameRooms, gameName) != 0)
		return NULL;
	GameRoom *gameRoom = (GameRoom *)ncListAdd(&GameRooms, gameName);
	if (gameRoom == NULL)
		return NULL;

	gameRoom->language = client->language;
	gameRoom->playerCount = 1;
	gameRoom->maxPlayers = maxPlayers;
	gameRoom->players[0] = client;
	gameRoom->status = GameChoosing;
	memcpy(gameRoom->gameParams, gameParams, 4);
	time(&gameRoom->creationTime);
	client->chatRoom = (ChatRoom *)gameRoom;
	client->status = CliInGameRoom;
	if (ncGameRoomNewCallback != NULL)
		(*ncGameRoomNewCallback)(gameRoom);
	NetMsgT19 msg;
	msg.head.msgId = gameRoom->listItem.id;
	msg.head.size = 36;
	msg.head.msgType = 19;
	msg.language = gameRoom->language;
	msg.maxPlayers = gameRoom->maxPlayers;
	msg.unk5_3 = 0;
	memcpy(&msg.gameParams, gameRoom->gameParams, 4);
	strcpy(msg.gameName, gameRoom->listItem.name);
	ncServerSendAllClientsTcpMsgExceptFrom(&msg.head, client->listItem.id);
	const char *timeString = ncGetTimeString(gameRoom->creationTime);
	ncLogPrintf(2, "New game room \'%s\' (ID=%d) created by client \'%s\' (ID=%d) at %s.",
			gameRoom->listItem.name, gameRoom->listItem.id, client->listItem.name,
			client->listItem.id, timeString);

	return gameRoom;
}

void ncGameSendMsgExceptFrom(GameRoom *gameRoom, NetMsg *msg, int userId)
{
	for (int i = 0; i < (int)gameRoom->playerCount; i++) {
		if (gameRoom->players[i] != NULL && gameRoom->players[i]->listItem.id != userId)
			ncServerSendClientTcpMsg(gameRoom->players[i], msg);
	}
}

void ncGameSetNewCallback(void *callback) {
	ncGameRoomNewCallback = callback;
}

void ncGameSetResultCallback(void *callback) {
	ncServerGameResultCallback = callback;
}

void ncGameRoomSetJoinCallback(void *callback) {
	ncGameRoomJoinCallback = callback;
}

void ncGameRoomSetQuitCallback(void *callback) {
	ncGameRoomQuitCallback = callback;
}

GameRoom *ncGameRoomJoin(Client *client, int gameId)
{
	GameRoom *gameRoom = (GameRoom *)ncListFindById(&GameRooms, gameId);
	if (gameRoom == NULL)
		return NULL;
	if (gameRoom->playerCount >= gameRoom->maxPlayers || gameRoom->status != GameChoosing)
		return NULL;
	NetMsgT20 msg20;
	msg20.head.msgId = client->listItem.id;
	msg20.head.size = 28;
	msg20.head.msgType = 20;
	strcpy(msg20.playerName, client->listItem.name);
	ncGameSendMsgExceptFrom(gameRoom, &msg20.head, client->listItem.id);
	client->chatRoom = (ChatRoom *)gameRoom;
	client->status = CliInGameRoom;
	gameRoom->players[gameRoom->playerCount] = client;
	gameRoom->playerReady[gameRoom->playerCount] = 0;
	gameRoom->playerParams[gameRoom->playerCount] = 0;
	gameRoom->pointsWon[gameRoom->playerCount] = 0;
	gameRoom->playerCount++;
	NetMsg msg21;
	msg21.msgId = gameRoom->listItem.id;
	msg21.size = 12;
	msg21.msgType = 21;
	ncServerSendAllClientsTcpMsgExceptFrom(&msg21, client->listItem.id);
	if (ncGameRoomJoinCallback != NULL)
		(*ncGameRoomJoinCallback)(gameRoom);

	return gameRoom;
}

void ncGameSendInfos(Client *client, int gameId)
{
	GameRoom *gameRoom = (GameRoom *)ncListFindById(&GameRooms, gameId);
	if (gameRoom == NULL) {
		ncServerSendClientTcpStandardMsg(client, 22, 0);
	}
	else
	{
		NetMsgT22 msg;
		memset(&msg, 0, sizeof(msg));
		msg.head.msgId = gameRoom->listItem.id;
		msg.head.size = 180;
		msg.head.msgType = 22;
		msg.language = gameRoom->language;
		msg.maxPlayers = gameRoom->maxPlayers;
		msg.playerCount = gameRoom->playerCount;
		msg.gameStatus = gameRoom->status;
		memcpy(msg.gameParams, gameRoom->gameParams, 4);
		for (int i = 0; i < (int)gameRoom->playerCount; i = i + 1)
		{
			strcpy(msg.players[i].name, gameRoom->players[i]->listItem.name);
			msg.players[i].playerParam = gameRoom->playerParams[i];
			msg.players[i].clientId = gameRoom->players[i]->listItem.id;
			msg.players[i].playerReady = gameRoom->playerReady[i];
			memcpy(msg.players[i].results, &gameRoom->results[i], sizeof(Result));
		}
		ncServerSendClientTcpMsg(client, &msg.head);
	}
}

void ncGameRoomQuit(Client *client)
{
	if (client == NULL || client->chatRoom == NULL)
		return;
	GameRoom *gameRoom = (GameRoom *)client->chatRoom;
	int idx;
	for (idx = 0; idx < (int)gameRoom->playerCount; idx++)
	{
		if (client == gameRoom->players[idx])
		{
			if (ncGameRoomQuitCallback != NULL)
				(*ncGameRoomQuitCallback)(client);
			NetMsg msg;
			msg.msgId = gameRoom->listItem.id;
			msg.msgType = 23;
			msg.size = 12;
			ncServerSendAllClientsTcpMsgExceptFrom(&msg, client->listItem.id);
			msg.msgId = client->listItem.id;
			msg.msgType = 24;
			ncGameSendMsgExceptFrom(gameRoom, &msg, client->listItem.id);
			client->chatRoom = NULL;
			if (client->status >= CliConnected)
				client->status = CliConnected;
			gameRoom->playerCount -= 1;
			break;
		}
	}
	for (; idx < (int)gameRoom->playerCount; idx++)
	{
		gameRoom->players[idx] = gameRoom->players[idx + 1];
		gameRoom->playerParams[idx] = gameRoom->playerParams[idx + 1];
		memcpy(&gameRoom->results[idx], &gameRoom->results[idx + 1], sizeof(Result));
		gameRoom->pointsWon[idx] = gameRoom->pointsWon[idx + 1];
		gameRoom->playerReady[idx] = gameRoom->playerReady[idx + 1];
	}
}

void ncGameSendList(Client *client)
{
	NetMsgT26 msg;
	memset(&msg, 0, sizeof(msg));
	msg.head.msgId = 0;
	msg.head.size = 232;
	msg.head.msgType = 26;
	msg.count = 0;
	for (GameRoom *gameRoom = (GameRoom *)GameRooms.head; gameRoom != NULL;
			gameRoom = (GameRoom *)gameRoom->listItem.next)
	{
		if (msg.count > 7) {
			ncServerSendClientTcpMsg(client,&msg.head);
			msg.count = 0;
		}
		msg.gameIds[msg.count] = gameRoom->listItem.id;
		msg.maxPlayers_gameStatus[msg.count] = *(uint16_t *)&gameRoom->maxPlayers;	// FIXME split maxPlayers_gameStatus
		msg.playerCounts[msg.count] = gameRoom->playerCount;
		strcpy(msg.gameNames[msg.count], gameRoom->listItem.name);
		memcpy(msg.gameParams[msg.count], gameRoom->gameParams, 4);
		msg.count++;
	}
	if (msg.count != 0)
		ncServerSendClientTcpMsg(client, &msg.head);
}

void ncGameRoomSetDeleteCallback(void *callback) {
	GameRooms.delItemCallback = callback;
}

void ncGameRoomDelete(GameRoom *gameRoom)
{
	ncServerSendAllClientsStandardTcpMsg(25, gameRoom->listItem.id);
	ncLogPrintf(2, "Game room \'%s\' (ID=%d) deleted.", gameRoom->listItem.name,
			gameRoom->listItem.id);
	int ret = ncListDelete(&GameRooms, &gameRoom->listItem);
	if (ret == 0)
		ncLogPrintf(2, "ERROR deleting Game room \'%s\' (ID=%d) deleted.", gameRoom->listItem.name,
				gameRoom->listItem.id);
}

void ncGameCheckLoad(GameRoom *gameRoom)
{
	for (int i = 0; i < (int)gameRoom->playerCount; i++)
		if (gameRoom->players[i]->status != CliWaiting)
			return;
	// Start game
	NetMsg msg;
	msg.msgId = LastRaceID;
	gameRoom->raceId = LastRaceID;
	LastRaceID++;
	if (LastRaceID == 0)
		LastRaceID = 1;
	msg.size = 12;
	msg.msgType = 32;
	for (int i = 0; i < (int)gameRoom->playerCount; i++)
	{
		gameRoom->players[i]->status = CliPlaying;
		gameRoom->players[i]->gameCount += 1;
		gameRoom->players[i]->lastUdpTimer = 0;
		ncServerSendClientTcpMsg(gameRoom->players[i], &msg);
	}
	gameRoom->status = GameRunning;
	ncGameStatusChanged(gameRoom);
}

void ncGameReadInGameMessage(Client *client, NetMsgT36 *msg)
{
	GameRoom *gameRoom = (GameRoom *)client->chatRoom;
	if ((unsigned)MaxUdpMsgSize < msg->head.size)
		MaxUdpMsgSize = msg->head.size;
	NetMsgT36 lmsg;
	if (gameRoom == NULL || gameRoom->status != GameRunning)
	{
		lmsg.head.msgId = 0;
		if (gameRoom == NULL)
			lmsg.raceId = 0;
		else
			lmsg.raceId = gameRoom->raceId;
		lmsg.head.size = 20;
		lmsg.head.msgType = 35;
		ncServerSendClientUdpMsg(client, &lmsg.head);
	}
	else if (msg->raceId == gameRoom->raceId)
	{
		if (gameRoom->playerCount == 1)
		{
			lmsg.head.msgId = 0;
			lmsg.udpSequence = 0;
			lmsg.raceId = gameRoom->raceId;
			lmsg.head.size = 20;
			lmsg.head.msgType = 37;
			ncServerSendClientUdpMsg(gameRoom->players[0], &lmsg.head);
		}
		else
		{
			for (int i = 0; i < gameRoom->playerCount; i++) {
				if (gameRoom->players[i] != client)
					ncServerSendClientUdpMsg(gameRoom->players[i], &msg->head);
			}
		}
	}
}

void ncGameReadUdpMsgCallback(NetMsgT36 *msg)
{
	ncNbUdpReceived++;
	Client *client = ncClientFindById(msg->head.msgId);
	if (client == NULL || client->status <= CliDisconnecting)
		return;
	client->lastUdpTimer = TimerRef;
	if (client->udpRecvSequence != 0 && msg->udpSequence - client->udpRecvSequence > 1)
	{
		int lost = msg->udpSequence - client->udpRecvSequence - 1;
		client->lostUdpPackets += lost;
		ncNbUdpLost += lost;
	}
	client->udpRecvSequence = msg->udpSequence;
	short msgType = msg->head.msgType;
	if (msgType == 2) {
		if (msg->head.msgId != 0)
			ncServerSendClientUdpMsg(client, &msg->head);
	}
	else if (msgType == 36) {
		ncGameReadInGameMessage(client, msg);
	}
}

void ncGameManageUdp(void) {
	ncSocketReadMsg(ncServerUdpSocket, (void (*)(uint8_t *, void *))ncGameReadUdpMsgCallback, NULL);
}

void ncGameCheckEnd(GameRoom *gameRoom)
{
	for (int i = 0; i < (int)gameRoom->playerCount; i++)
		if (gameRoom->players[i]->status != CliEnding)
			return;
	// End game
	if (ncServerGameResultCallback != NULL)
		(*ncServerGameResultCallback)(gameRoom);
	gameRoom->status = GameChoosing;
	ncGameStatusChanged(gameRoom);
	for (int i = 0; i < (int)gameRoom->playerCount; i++)
	{
		Client *client = gameRoom->players[i];
		client->status = CliInGameRoom;
		if (client->timer == 0)
			client->timer = TimerRef + 5000;
	}
}

static void gameRoomManage(GameRoom *gameRoom, void *arg)
{
	int repeat;
	do {
		repeat = 0;
		for (int i = 0; i < (int)gameRoom->playerCount; i++)
		{
			Client *client = gameRoom->players[i];
			if (Udp_Timeout != 0
					&& client->status == CliPlaying
					&& client->lastUdpTimer != 0
					&& TimerRef - client->lastUdpTimer > Udp_Timeout)
			{
				ncLogPrintf(1, "!!!TIMEOUT : client \'%s\' (ID=%d) UDP timeout !", client->listItem.name,
						client->listItem.id);
				ncServerDisconnectClient(client);
			}
			if (client->status <= CliDisconnecting)
			{
				ncGameRoomQuit(gameRoom->players[i]);
				repeat = 1;
				break;
			}
		}
	} while (repeat != 0);
	if (gameRoom->playerCount == 0)
		ncGameRoomDelete(gameRoom);
	else if (gameRoom->status == GameLoading)
		ncGameCheckLoad(gameRoom);
	else if (gameRoom->status == GameResults)
		ncGameCheckEnd(gameRoom);
}

void ncGameRoomManage(void) {
	ncServerEnumGameRooms(gameRoomManage, NULL);
}

void ncGameSetParams(Client *client, NetMsgT27 *msg)
{
	GameRoom *gameRoom = (GameRoom *)client->chatRoom;
	if (gameRoom != NULL)
	{
		gameRoom->maxPlayers = msg->maxPlayers;
		memcpy(gameRoom->gameParams, msg->gameParams, 4);
		ncServerSendAllClientsTcpMsgExceptFrom(&msg->head, client->listItem.id);
	}
}

void ncGameSetPlayerParams(Client *client, NetMsgT28 *msg)
{
	GameRoom *gameRoom = (GameRoom *)client->chatRoom;
	if (gameRoom == NULL)
		return;
	for (int idx = 0; idx < (int)gameRoom->playerCount; idx++)
	{
		if (gameRoom->players[idx] == client)
		{
			gameRoom->playerParams[idx] = msg->playerParams;
			ncGameSendMsgExceptFrom(gameRoom, (NetMsg *)msg, client->listItem.id);
			break;
		}
	}
}

void ncGameStartLoading(Client *client, NetMsgT30 *msg)
{
	GameRoom *gameRoom = (GameRoom *)client->chatRoom;
	if (gameRoom == NULL)
		return;
	memcpy(gameRoom->gameParams, &msg->gameParams, 4);
	for (int idx = 0; idx < (int)gameRoom->playerCount; idx++)
	{
		if (idx < (int)msg->nplayers)
		{
			gameRoom->players[idx]->status = CliLoading;
			if (client->listItem.id != gameRoom->players[idx]->listItem.id)
				ncServerSendClientTcpMsg(gameRoom->players[idx], &msg->head);
		}
		else {
			ncServerSendClientTcpStandardMsg(gameRoom->players[idx], 38, 0);
		}
	}
	gameRoom->status = GameLoading;
	ncGameStatusChanged(gameRoom);
}

void ncGamePlayerResults(Client *client, NetMsgT34 *msg)
{
	GameRoom *gameRoom = (GameRoom *)client->chatRoom;
	if (gameRoom == NULL)
		return;
	client->status = CliEnding;
	for (int idx = 0; idx < (int)gameRoom->playerCount; idx++)
	{
		if (gameRoom->players[idx] == client)
		{
			memcpy(&gameRoom->results[idx], msg->results, sizeof(Result));
			ncGameSendMsgExceptFrom(gameRoom, (NetMsg *)msg, client->listItem.id);
			break;
		}
	}
	if (gameRoom->status != GameResults)
	{
		for (int idx = 0; idx < (int)gameRoom->playerCount; idx++)
			gameRoom->playerReady[idx] = 0;
		gameRoom->status = GameResults;
		ncGameStatusChanged(gameRoom);
	}
}

void ncGameKickClient(GameRoom *gameRoom,int clientId)
{
	int i = 0;
	for (;;)
	{
		if (i >= (int)gameRoom->playerCount)
			return;
		if (gameRoom->players[i]->listItem.id == clientId)
			break;
		i++;
	}
	ncGameRoomQuit(gameRoom->players[i]);
}

void ncGameSetPlayerReady(Client *client, NetMsg *msg)
{
	GameRoom *gameRoom = (GameRoom *)client->chatRoom;
	if (gameRoom == NULL)
		return;
	for (int idx = 0; idx < (int)gameRoom->playerCount; idx++)
	{
		if (gameRoom->players[idx] == client)
		{
			gameRoom->playerReady[idx] = 1;
			client->status = CliReady;
			ncGameSendMsgExceptFrom(gameRoom, msg, client->listItem.id);
			break;
		}
	}
}

void ncServerEnumGameRooms(void (*callback)(GameRoom *gameRom, void *arg), void *arg) {
	ncListEnum(&GameRooms, (ListEnumCallback)callback, arg);
}

const char *ncGetTimeString(int time_)
{
	static char TmpTime[32];
	time_t t;
	struct tm *tm;

	t = time_;
	if (time_ == 0) {
		time(&t);
	}
	tm = localtime(&t);
	sprintf(TmpTime,"%02d:%02d:%02d",tm->tm_hour,tm->tm_min,tm->tm_sec);
	return TmpTime;
}

const char *ncConvertSeconds(uint32_t totsecs)
{
	static char TmpTime[32];

	uint32_t mins = (totsecs % 3600) / 60;
	uint32_t secs = (totsecs % 3600) % 60;
	if (totsecs / 3600 == 0)
	{
		if (mins == 0)
			sprintf(TmpTime, "%d sec", secs);
		else
			sprintf(TmpTime, "%d min, %02d sec", mins, secs);
	}
	else {
		sprintf(TmpTime, "%d h, %02d min, %02d sec", totsecs / 3600, mins, secs);
	}
	return TmpTime;
}
