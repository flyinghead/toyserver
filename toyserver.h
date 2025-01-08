#pragma once
#include "ncList.h"
#include "ncCRC32.h"
#include <stdint.h>
#include <stdio.h>
#include <netinet/in.h>
#include <time.h>
#include <string.h>
#include <sys/select.h>

#include "netmsg.h"

#define SERVER_VERSION 9

struct Socket
{
    int fd;
    int recvBufSize;
    uint8_t recvBuf[1024];
    int sendBufSize;
    uint8_t sendBuf[1024];
};
typedef struct Socket Socket;

struct ChatRoom
{
	ListItem listItem;
    uint16_t language;
    uint16_t userCount;
    time_t creationTime;
    struct Client *clients[16];
};
typedef struct ChatRoom ChatRoom;

typedef enum ClientStatus
{
    CliAvailable = 0,
    CliDisconnecting = 1,
    CliConnected = 2,
    CliChatting = 3,
    CliInGameRoom = 4,
    CliReady = 5,
    CliLoading = 6,
    CliWaiting = 7,
    CliPlaying = 8,
    CliEnding = 9
} ClientStatus;

struct Client
{
	ListItem listItem;
    Socket sock;
    in_addr_t ipAddress;
    uint16_t tcpPort;
    uint16_t udpPort;
    enum ClientStatus status;
    uint8_t language;
    uint8_t unk5;
    struct ChatRoom *chatRoom;
    time_t connectTime;
    time_t timer;
    time_t lastUdpTimer;
    uint32_t udpRecvSequence;
    int lostUdpPackets;
    uint16_t gameCount;
    uint16_t racesWon;
};
typedef struct Client Client;

struct Result
{
    int totalSecs;
    int bestLap;
    int unk1;
    int unk2;
};
typedef struct Result Result;

typedef enum eGameStatus
{
    GameChoosing = 0,
    GameLoading = 1,
    GameRunning = 2,
    GameResults = 3
} eGameStatus;

struct GameRoom
{
	ListItem listItem;
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

struct ChatInfo
{
    int roomId;
    int language;
    int msgCount;
    char chatName[16];
    char highScoreFileName[256];
    char messages[100][256];
};
typedef struct ChatInfo ChatInfo;

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

extern time_t TimerRef;

int ncServerGetClientUdpTimeout(void);
void ncServerGetClientTimers(uint16_t *dynPackPerSec, uint16_t *cmdPackPerSec);
uint32_t ComputeMsgCRC(NetMsg *data);
int ncSocketTcpSendStandardMsg(int sockfd, uint16_t param_2, int param_3);
int ncSocketTcpSendMsg(Socket *socket, NetMsg *msg);
int ncSocketUdpSendMsg(int sockfd, in_addr_t dstaddr, uint16_t dstport, NetMsg *msg);
ssize_t ncSocketReadMsg(int sockfd, void (*callback)(uint8_t *data, void *), void *cbArg);
ssize_t ncSocketBufReadMsg(Socket *socket, void (*callback)(uint8_t *data, void *), void *cbArg);
ChatRoom *ncServerGetChatById(int roomId);
int ncServerGetNbInfoChats(void);
ChatInfo *ncServerGetInfoChatIndexPtr(int chatIdx);
int ncServerCreateInfoChat(ChatInfo *srcChatInfo);
int ncChatInfoJoin(Client *client, int roomId);
int ncChatRoomAllocate(int maxChatRooms);
void ncChatRoomRelease(void);
ChatRoom *ncChatRoomCreate(Client *client, char *roomName);
void ncChatSendMsgExceptFrom(ChatRoom *chatRoom, NetMsg *msg, int senderId);
void ncChatSendInfos(Client *client, int roomId);
ChatRoom *ncChatRoomJoin(Client *client, int roomId);
void ncChatRoomQuit(Client *client);
void ncChatRoomSetNewCallback(void *callback);
void ncChatRoomSetJoinCallback(void *callback);
void ncChatRoomSetQuitCallback(void *callback);
void ncChatRoomSetDeleteCallback(void *callback);
void ncChatRoomDelete(ChatRoom *room);
void ncChatSendList(Client *client);
void ncChatRoomManage(void);
void ncChatKickClient(int param_1, int param_2);
void ncServerEnumChatRooms(void (*callback)(ChatRoom *, void *), void *arg);
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
Client *ncClientFindById(int clientId);
void ncServerEnumClients(void (*callback)(Client *, void *), void *arg);
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
GameRoom *ncServerGetGameById(int gameId);
void ncGameStatusChanged(GameRoom *gameRoom);
void ncServerSetUdpTimeout(int timeout);
int ncGameRoomAllocate(int maxGameRooms);
void ncGameRoomRelease(void);
GameRoom *ncGameRoomCreate(Client *client, char *gameName, uint8_t *gameParams, uint8_t maxPlayers);
void ncGameSendMsgExceptFrom(GameRoom *gameRoom, NetMsg *msg, int userId);
void ncGameSetNewCallback(void *callback);
void ncGameSetResultCallback(void *callback);
void ncGameRoomSetJoinCallback(void *callback);
void ncGameRoomSetQuitCallback(void *callback);
GameRoom *ncGameRoomJoin(Client *client, int gameId);
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
void ncServerEnumGameRooms(void (*callback)(GameRoom *gameRom, void *arg), void *arg);
const char *ncGetTimeString(int time);
const char *ncConvertSeconds(uint32_t totsecs);
void ncLogWrite(int logType, char *msg);
void ncLogPrintf(int logType, char *format, ...);
int ncServerStartListening(uint16_t tcpport);
void ncServerStopListening(void);
void ncServerRemoveSocket(int idx);
void ncServerSetLoginCallback(void *callback);
int ncServerSocketInit(uint16_t tcpPort, uint16_t udpPort);
void ncServerSocketRelease(void);
int ncSocketGetLastError(void);
void ncSocketError(int error);
int ncSocketInit(void);
void ncSocketRelease(void);
void ncSocketClose(int *pSockfd);
uint8_t *ncSocketRead(int sockfd, ssize_t *len);
int ncSocketTcpSend(Socket *socket, void *data, size_t len);
char * ncGetAddrString(in_addr_t addr);
int ncSocketUdpInit(uint16_t port);
int ncSocketUdpSend(int sockfd, void *data, size_t len, in_addr_t dst_addr, uint16_t dstport);
void _strdate(char *date);
void asyncRead(int fd, void(*callback)(void*), void *argument);
void asyncWrite(int fd, void(*callback)(void*), void *argument);
void cancelAsyncRead(int fd);
void cancelAsyncWrite(int fd);
int pollWait(int timeout);
