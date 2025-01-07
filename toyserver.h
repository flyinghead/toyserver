#pragma once
#include "ncList.h"
#include "ncCRC32.h"
#include <stdint.h>
#include <stdio.h>
#include <netinet/in.h>
#include <time.h>
#include <string.h>

#include "netmsg.h"

struct Socket {
    int sockfd;
    int recvBufSize;
    uint8_t recvBuf[10240];	// TODO way too much
    int sendBufSize;
    uint8_t sendBuf[10240];	// TODO way too much
    int timer;
    int srcAddr;
    short tcpPort;
    short loginExpected;
};
typedef struct Socket Socket;

struct ChatRoom {
    struct ListItem listItem;
    uint16_t language;
    uint16_t userCount;
    time_t creationTime;
    struct Client *clients[16];
};
typedef struct ChatRoom ChatRoom;

typedef enum ClientStatus {
    CliAvailable=0,
    CliDisconnecting=1,
    CliConnected=2,
    CliChatting=3,
    CliInGameRoom=4,
    CliReady=5,
    CliLoading=6,
    CliWaiting=7,
    CliPlaying=8,
    CliEnding=9
} ClientStatus;

struct Client {
    struct ListItem listItem;
    int sockfd;
    int recvBufSize;
    uint8_t recvBuf[10240];	// TODO way too much
    int sendBufSize;
    uint8_t sendBuf[10240];	// TODO way too much
    in_addr_t ipAddress;
    uint16_t tcpPort;
    uint16_t udpPort;
    enum ClientStatus status;
    uint8_t language;
    uint8_t unk5;
    struct ChatRoom *chatRoom;
    time_t connectTime;
    int tcpRecvMsg;
    int tcpSentMsg;
    uint32_t timer;
    int cumPingTime;
    int pingRecv;
    int unk9;
    int lastUdpTimer;
    int udpRecvMsg;
    int udpSentMsg;
    int udpRecvSequence;
    int lostUdpPackets;
    uint16_t gameCount;
    uint16_t racesWon;
};
typedef struct Client Client;

struct Result {
    int totalSecs;
    int bestLap;
    int unk1;
    int unk2;
};
typedef struct Result Result;

typedef enum eGameStatus {
    GameChoosing=0,
    GameLoading=1,
    GameRunning=2,
    GameResults=3
} eGameStatus;

struct GameRoom {
    struct ListItem listItem;
    int raceId;
    time_t creationTime;
    uint8_t language;
    uint8_t playerCount;
    uint8_t maxPlayers;
    enum eGameStatus status;
    struct Client *players[4];
    uint8_t playerParams[4];
    struct Result results[4];
    uint8_t playerReady[4];
    uint8_t gameParams[4];	// gameParams[3] & 0x7f is track id
    uint8_t pointsWon[4];	// 1st: 3 points, 2nd: 2 points, 3rd: 1 pointt
};
typedef struct GameRoom GameRoom;

struct BestLap {
    int time;
    char name[16];
};
typedef struct BestLap BestLap;

struct ChatInfo {
    int roomId;
    int language;
    int msgCount;
    char chatName[16];
    char highScoreFileName[256];
    char messages[100][256];
};
typedef struct ChatInfo ChatInfo;

struct Stats {
    uint32_t totalConnections;
    uint32_t maxConnections;
    time_t maxConnectionsTime;
    uint32_t connectionCount;
    uint32_t totalGames;
    uint32_t maxGames;
    time_t maxGamesTimes;
    uint32_t gameCount;
    uint32_t totalChats;
    uint32_t maxChats;
    time_t maxChatsTime;
    uint32_t chatCount;
    time_t connectionTime;
    time_t gameTime;
    time_t chatTime;
};
typedef struct Stats Stats;

struct HighScores {
	int times[8];
	char RacerNames[8][16];
	uint8_t RacerCars[8];
	char RacerCountries[8][10];
};
typedef struct HighScores HighScores;

extern int MaxNbChats;
extern int MaxNbGames;

extern int ncServerUdpSocket;
extern uint16_t ncServerUdpPort;
extern int ncNbTcpSent;
extern int ncNbTcpReceived;
extern int ncNbUdpReceived;
extern int ncNbUdpSent;
extern int ncNbUdpLost;
extern int MaxUdpMsgSize;

extern int TimerRef;

void ManageTime(void);
void RefreshInfo(int roomId, int verbose);
void ConvertTime(unsigned time_ms, unsigned *mins, unsigned *secs, unsigned *hundreths);
void ReadHighScoreFile(void);
void HighScoresChatFile(FILE *fp, HighScores *data);
void SaveHighScoreFile(int refreshInfo);
void ReadClientInfosMessage(NetMsgT7 *msg);
void ResetDailyStats(void);
void SaveDailyStats(void);
int ncServerGetClientUdpTimeout(void);
void ncServerGetClientTimers(short *dynPackPerSec, short *cmdPackPerSec);
void NewInfoChatRoom(void);
int ReadCfgInt(char *start, char *end, char **value);
int ReadCfgInfoChat(char *start, char *end, char **value);
int CreateInfoHighScores(void);
int ReadCfgFile(char *filename);
void NewClientCallback(void);
void DisconnectClientCallback(Client *client);
void NewChatRoomCallback(void);
void DeleteChatRoomCallback(ChatRoom *chatRoom);
void NewGameCallback(void);
void DeleteGameCallback(GameRoom *gameRoom);
void ResultCallback(GameRoom *gameRoom);
int ServerStart(void);
void ServerManage(void);
void ServerShutDown(void);
int CheckInfoChatFile(char *fileName);
void DisplayStats(void);
void DisplayHigh(void);
void GameListCallback(GameRoom *gameRoom);
void ChatListCallback(ChatRoom *chatRoom);
void PlayerListCallback(Client *client);
void DisplayChatInfo(int roomId);
void DisplayGameInfo(int gameId);
void DisplayPlayerInfo(int clientId);
void ListInfos(void);
void DisplayInfoInfo(int roomId);
void ServerCommand(char *cmd);
uint32_t ComputeMsgCRC(NetMsg *data);
int ncSocketTcpSendStandardMsg(int sockfd, uint16_t param_2, int param_3);
int ncSocketTcpSendMsg(Socket *socket, NetMsg *msg, int useSocketBuffer);
int ncSocketUdpSendMsg(int sockfd, in_addr_t dstaddr, uint16_t dstport, NetMsg *msg);
ssize_t ncSocketReadMsg(int sockfd, void (*callback)(uint8_t *data, void *), void *cbArg);
ssize_t ncSocketBufReadMsg(Socket *socket, void (*callback)(uint8_t *data, void *), void *cbArg);
ChatRoom * ncServerGetChatById(int roomId);
int ncServerGetNbInfoChats(void);
ChatInfo * ncServerGetInfoChatIndexPtr(int chatIdx);
int ncServerCreateInfoChat(ChatInfo *srcChatInfo);
void ncServerDeleteInfoChat(int roomId);
int ncChatInfoJoin(Client *client, int roomId);
int ncChatRoomAllocate(int maxChatRooms);
void ncChatRoomRelease(void);
ChatRoom * ncChatRoomCreate(Client *client, char *roomName);
void ncChatSendMsgExceptFrom(ChatRoom *chatRoom, NetMsg *msg, int senderId);
void ncChatSendInfos(Client *client, int roomId);
ChatRoom * ncChatRoomJoin(Client *client, int roomId);
void ncChatRoomQuit(Client *client);
void ncChatRoomSetNewCallback(void *callback);
void ncChatRoomSetJoinCallback(void *callback);
void ncChatRoomSetQuitCallback(void *callback);
void ncChatRoomSetDeleteCallback(void *callback);
void ncChatRoomDelete(ChatRoom *room);
void ncChatSendList(Client *client);
void ncChatRoomManage(void);
void ncChatKickClient(int param_1, int param_2);
void ncServerEnumChatRooms(void *callback);
void ncServerRemoveClient(Client *client);
void ncServerDisconnectClient(Client *client);
int ncServerSendClientTcpMsg(Client *client, NetMsg *msg);
int ncServerSendClientUdpMsg(Client *client, NetMsg *msg);
void ncServerSendAllClientsStandardTcpMsg(short msgType, int msgId);
int ncServerSendClientTcpStandardMsg(Client *client, short msgType, int msgId);
int ncServerLoginCallback(int sockfd, uint32_t srcIp, uint16_t tcpPort, NetMsgT1 *msg);
int ncServerClientInit(void);
int ncServerClientAllocate(int maxPlayers);
void ncServerClientRelease(void);
void ncServerSetNewClientCallback(void *callback);
void ncServerSetDisconnectClientCallback(void *callback);
void ncServerSendAllClientsTcpMsgExceptFrom(NetMsg *msg, int clientId);
void ncServerSendAllClientsTcpMsgExceptFromAndRoom(NetMsg *msg, int clientId, ChatRoom *chatRoom);
void ncServerManageClients(void);
Client * ncClientFindById(int clientId);
void ncServerEnumClients(void *callback);
void ncServerClientReadClientInfosMessage(Client *client, NetMsg *msg);
void ncServerClientSetInfosCallback(void *callback);
void ncServerClientReadPingMessage(Client *client, NetMsgT2 *msg);
void ncServerClientReadCreateRoomMessage(Client *client, NetMsgT8 *msg);
void ncServerClientReadJoinRoomMessage(Client *client, NetMsg *msg);
void ncServerClientReadChatMessage(Client *client, NetMsg *msg);
void ncServerClientReadCreateGameMessage(Client *client, NetMsgT18 *msg);
void ncServerClientReadJoinGameMessage(Client *client, NetMsg *msg);
void ncServerClientReadKickMessage(Client *client, NetMsg *msg);
void ncServerClientReadMsgCallback(NetMsg *msg, Client *client);
GameRoom * ncServerGetGameById(int gameId);
void ncGameStatusChanged(GameRoom *gameRoom);
void ncServerSetUdpTimeout(int timeout);
int ncGameRoomAllocate(int maxGameRooms);
void ncGameRoomRelease(void);
GameRoom * ncGameRoomCreate(Client *client, char *gameName, uint8_t *gameParams, uint8_t maxPlayers);
void ncGameSendMsgExceptFrom(GameRoom *gameRoom, NetMsg *msg, int userId);
void ncGameSetNewCallback(void *callback);
void ncGameSetResultCallback(void *callback);
void ncGameRoomSetJoinCallback(void *callback);
void ncGameRoomSetQuitCallback(void *callback);
GameRoom * ncGameRoomJoin(Client *client, int gameId);
void ncGameSendInfos(Client *client, int gameId);
void ncGameRoomQuit(Client *client);
void ncGameSendList(Client *client);
void ncGameRoomSetDeleteCallback(void *callback);
void ncGameRoomDelete(GameRoom *gameRoom);
void ncGameCheckLoad(GameRoom *gameRoom);
void ncGameReadInGameMessage(Client *client, NetMsgT36 *msg);
void ncGameReadUdpMsgCallback(NetMsgT36 *msg);
void ncGameManageUdp(void);
void ncGameCheckEnd(GameRoom *gameRoom);
void ncGameRoomManage(void);
void ncGameSetParams(Client *client, NetMsgT27 *msg);
void ncGameSetPlayerParams(Client *client, NetMsgT28 *msg);
void ncGameStartLoading(Client *client, NetMsgT30 *msg);
void ncGamePlayerResults(Client *client, NetMsgT34 *msg);
void ncGameKickClient(GameRoom *gameRoom, int clientId);
void ncGameSetPlayerReady(Client *client, NetMsg *msg);
void ncServerEnumGameRooms(void *callback);
const char *ncGetTimeString(int time);
const char *ncConvertSeconds(uint32_t totsecs);
void ncLogWrite(int logType, char *msg);
void ncLogPrintf(int logType, char *format, ...);
int ncServerStartListening(uint16_t tcpport);
void ncServerStopListening(void);
void ncServerManageConnections(void);
void ncServerRemoveSocket(int idx);
void ncWaitingSocketMsgCallback(uint8_t *data, Socket *socket);
void ncServerManageLogin(void);
void ncServerSetLoginCallback(void *callback);
int ncServerSocketInit(uint16_t tcpPort, uint16_t udpPort);
void ncServerSocketRelease(void);
int ncSocketGetLastError(void);
void ncSocketError(int error);
int ncSocketInit(void);
void ncSocketRelease(void);
void ncSocketClose(int *pSockfd);
uint8_t * ncSocketRead(int sockfd, ssize_t *len);
int ncSocketTcpSend(Socket *socket, void *data, size_t len, int useSendBuffer);
char * ncGetAddrString(in_addr_t addr);
int ncSocketUdpInit(uint16_t port);
int ncSocketUdpSend(int sockfd, void *data, size_t len, in_addr_t dst_addr, uint16_t dstport);
void _strdate(char *date);
