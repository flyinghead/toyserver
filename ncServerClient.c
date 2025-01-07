#include "toyserver.h"
#include <time.h>

static List Clients;
static int MaxNbClients;

static void (*ncServerClientDisconnectCallback)(Client *);
static void (*ncServerClientNewCallback)(Client *);
static void (*ncServerClientInfoCallback)(NetMsgT7 *);

static void clientSocketCallback(void *arg);

void ncServerRemoveClient(Client *client) {
	ncListDelete(&Clients, (ListItem *)client);
}

void ncServerDisconnectClient(Client *client)
{
	if (client->status >= CliConnected)
	{
		if (ncServerClientDisconnectCallback != NULL)
			(*ncServerClientDisconnectCallback)(client);
		client->status = CliDisconnecting;
		char *ip = ncGetAddrString(client->ipAddress);
		ncLogPrintf(1,"DISCONNECT : Client \'%s\' (ID=%d) IP %s , %d, udp %d on socket %d.",
				client->listItem.name, client->listItem.id, ip, ntohs(client->tcpPort), client->udpPort, client->sockfd);
		ncSocketClose(&client->sockfd);
	}
}

int ncServerSendClientTcpMsg(Client *client, NetMsg *msg)
{
	if (client == NULL || msg == NULL)
		return 0;
	if (client->status <= CliDisconnecting)
		return 0;
	// FIXME passing part of Client as Socket???
	if (ncSocketTcpSendMsg((Socket *)&client->sockfd, msg, 1) == 0)
	{
		ncLogPrintf(1, "ERROR sending TCP message to client \'%s\' (ID=%d).", client->listItem.name,
				client->listItem.id);
		ncServerDisconnectClient(client);
		return 0;
	}
	client->tcpSentMsg++;
	return 1;
}

int ncServerSendClientUdpMsg(Client *client, NetMsg *msg)
{
	if (client == NULL || msg == NULL)
		return 0;
	if (client->status <= CliDisconnecting)
		return 0;
	int rc = ncSocketUdpSendMsg(ncServerUdpSocket, client->ipAddress, client->udpPort, msg);
	if (rc == 0) {
		ncLogPrintf(1, "ERROR sending UDP message to client \'%s\' (ID=%d).", client->listItem.name,
				client->listItem.id);
		return 0;
	}
	client->udpSentMsg++;
	return 1;
}

void ncServerSendAllClientsStandardTcpMsg(short msgType, int msgId)
{
	NetMsg msg;
	msg.msgType = msgType;
	msg.msgId = msgId;
	msg.size = 12;
	for (Client *client = (Client *)Clients.head; client != NULL;
			client = (Client *)client->listItem.next) {
		ncServerSendClientTcpMsg(client, &msg);
	}
}

int ncServerSendClientTcpStandardMsg(Client *client, short msgType, int msgId)
{
	if (client == NULL)
		return 0;
	NetMsg msg;
	msg.msgType = msgType;
	msg.msgId = msgId;
	msg.size = 12;
	return ncServerSendClientTcpMsg(client, &msg);
}

int ncServerLoginCallback(int sockfd, uint32_t srcIp, uint16_t tcpPort, NetMsgT1 *msg)
{
	if (msg->unk1 == 9)
	{
		if (msg->head.msgId == 0)
		{
			if (Clients.length < Clients.capacity)
			{
				int bufsize = 0x4000;
				if (setsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, &bufsize, 4) == -1)
				{
					ncLogPrintf(1, "ERROR ncServerLoginCallback()-> setsockopt( SO_SNDBUF ) failed : ");
					ncSocketError(ncSocketGetLastError());
					ncServerSocketRelease();
					return 0;
				}
				if (setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, &bufsize, 4) == -1)
				{
					ncLogPrintf(1, "ERROR ncServerLoginCallback()-> setsockopt( SO_RCVBUF ) failed : ");
					ncSocketError(ncSocketGetLastError());
					ncServerSocketRelease();
					return 0;
				}
				char userName [16];
				strcpy(userName, msg->userName);
				int i = 0;
				while (ncListCheckNameUsed(&Clients, userName) != 0)
				{
					i = i + 1;
					if (999 < i) {
						ncSocketTcpSendStandardMsg(sockfd, 4, 0);
						return 0;
					}
					if (i != 0)
						// the game limits user names to 10 chars max
						sprintf(userName, "%.10s_%03d", msg->userName, i);
				}
				Client *client = (Client *)ncListAdd(&Clients, userName);
				if (client == NULL)
					return 0;
				client->sockfd = sockfd;
				client->status = CliConnected;
				client->ipAddress = srcIp;
				client->tcpPort = tcpPort;
				client->udpPort = msg->udpPort;
				client->language = msg->language;
				client->unk5 = msg->cli_unk5;
				time(&client->connectTime);
				client->tcpRecvMsg = 2;
				client->tcpSentMsg = 1;
				client->pingRecv = 0;
				client->cumPingTime = 0;
				client->timer = TimerRef + 5000;
				const char *timestr = ncGetTimeString(client->connectTime);
				char *addrstr = ncGetAddrString(client->ipAddress);
				ncLogPrintf(1,"New client \'%s\' (ID=%d) from %s , %d, udp %d on socket %d at %s.",
						client->listItem.name, client->listItem.id, addrstr, ntohs(tcpPort), client->udpPort, client->sockfd, timestr);
				stopPollingSocket(sockfd);
				pollReadSocket(sockfd, clientSocketCallback, client);
				if (ncServerClientNewCallback != NULL)
					(*ncServerClientNewCallback)(client);
				strcpy(msg->userName, client->listItem.name);
				msg->udpPort = ncServerUdpPort;
				msg->head.msgId = client->listItem.id;
				msg->udpTimeout = ncServerGetClientUdpTimeout();
				ncServerGetClientTimers(&msg->dynPackPerSec, &msg->cmdPackPerSec);
				if (ncServerSendClientTcpMsg(client, (NetMsg *)msg) != 0) {
					ncChatSendList(client);
					ncGameSendList(client);
				}
				return 1;
			}
			else
			{
				ncLogPrintf(1, "!!! SERVER IS FULL connection from %s , %d on socket %d rejected !!!",
						ncGetAddrString(srcIp), ntohs(tcpPort), sockfd);
				ncSocketTcpSendStandardMsg(sockfd, 6, 0);
			}
		}
		else {
			ncSocketTcpSendStandardMsg(sockfd, 4, 0);
		}
	}
	else {
		ncSocketTcpSendStandardMsg(sockfd, 41, 9);
	}
	return 0;
}

int ncServerClientInit(void) {
	ncServerSetLoginCallback(ncServerLoginCallback);
	return 1;
}

int ncServerClientAllocate(int maxPlayers) {
	MaxNbClients = maxPlayers;
	return ncListAllocate(&Clients, maxPlayers, sizeof(Client));
}

void ncServerClientRelease(void) {
	ncListRelease(&Clients);
	return;
}

void ncServerSetNewClientCallback(void *callback) {
	ncServerClientNewCallback = callback;
}

void ncServerSetDisconnectClientCallback(void *callback) {
	ncServerClientDisconnectCallback = callback;
}

void ncServerSendAllClientsTcpMsgExceptFrom(NetMsg *msg, int clientId)
{
	for (Client *client = (Client *)Clients.head; client != NULL;
			client = (Client *)client->listItem.next) {
		if (client->listItem.id != clientId)
			ncServerSendClientTcpMsg(client, msg);
	}
}

void ncServerSendAllClientsTcpMsgExceptFromAndRoom(NetMsg *msg, int clientId, ChatRoom *chatRoom)
{
	for (Client *client = (Client *)Clients.head; client != NULL;
			client = (Client *)client->listItem.next) {
		if (client->listItem.id != clientId && client->chatRoom != chatRoom)
			ncServerSendClientTcpMsg(client, msg);
	}
}

// FIXME to delete. But there's some logic left that needs to go somewhere
void ncServerManageClients(void)
{
	Client *client = (Client *)Clients.head;
	while (client != NULL)
	{
		Client *nextClient = (Client *)client->listItem.next;
		if (client->status <= CliDisconnecting) {
			if (client->chatRoom == NULL)
				ncServerRemoveClient(client);
		}
		client = nextClient;
	}
}

static void clientSocketCallback(void *arg)
{
	Client *client = (Client *)arg;
	// FIXME Client to Socket hack
	ssize_t len = ncSocketBufReadMsg((Socket *)&client->sockfd, (void (*)(uint8_t *, void *))ncServerClientReadMsgCallback, client);
	if (len < 0) {
		ncServerDisconnectClient(client);
	}
	else if (client->status >= CliConnected)		// FIXME should this go elsewhere? won't be run if nothing is received from the sock
	{
		if (client->sendBufSize == 0 || client->sockfd == -1)	// FIXME if client->sockfd == -1, we can't send a message...
		{
			if (client->timer != 0 && client->timer < TimerRef)
			{
				client->timer = 0;
				NetMsgT2 pingMsg;
				pingMsg.timer = TimerRef;
				pingMsg.head.size = 16;
				pingMsg.head.msgType = 2;
				pingMsg.head.msgId = 0;
				ncServerSendClientTcpMsg(client, (NetMsg *)&pingMsg);
			}
		}
		else
		{
			struct timeval tv;
			tv.tv_sec = 0;
			tv.tv_usec = 0;
			fd_set fdset;
			FD_ZERO(&fdset);
			FD_SET(client->sockfd, &fdset);
			ssize_t rc = select(client->sockfd + 1, NULL, &fdset, NULL, &tv);
			if (rc == -1) {
				ncServerDisconnectClient(client);
			}
			else if (rc > 0)
			{
				rc = send(client->sockfd, client->sendBuf, client->sendBufSize, MSG_NOSIGNAL);
				if (rc == -1)
				{
					ncLogPrintf(0, "ERROR : ncServerManageClients() : Can\'t send buffer data -> send(%d) error ",
							client->sockfd);
					ncSocketError(ncSocketGetLastError());
					ncServerDisconnectClient(client);
				}
				else if (rc == client->sendBufSize) {
					client->sendBufSize = 0;
					ncLogPrintf(0, "!!! SUCCESS !!! ncServerManageClients() : buffered data successfully sent !");
				}
				else
				{
					ncLogPrintf(0, "ERROR : ncServerManageClients() : Can\'t send buffer data -> not all data sent !");
					ncSocketError(ncSocketGetLastError());
					ncServerDisconnectClient(client);
				}
			}
		}
	}
}

Client *ncClientFindById(int clientId) {
	return (Client *)ncListFindById(&Clients, clientId);
}

void ncServerEnumClients(void (*callback)(Client *, void *), void *arg) {
	ncListEnum(&Clients, (ListEnumCallback)callback, arg);
}


void ncServerClientReadClientInfosMessage(Client *client, NetMsg *msg) {
	if (msg->size == 472 && ncServerClientInfoCallback != NULL)
		(*ncServerClientInfoCallback)((NetMsgT7 *)msg);
}

void ncServerClientSetInfosCallback(void *callback) {
	ncServerClientInfoCallback = callback;
}

