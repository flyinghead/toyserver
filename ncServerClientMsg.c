#include "toyserver.h"

int ncNbTcpReceived;

void ncServerClientReadPingMessage(Client *client, NetMsgT2 *msg)
{
	if (msg->head.msgId == 0)
	{
		client->pingRecv = client->pingRecv + 1;
		client->cumPingTime = client->cumPingTime + (TimerRef - msg->timer);
		if (client->status < CliLoading)
			client->timer = TimerRef + 5000;
	}
	else {
		ncServerSendClientTcpMsg(client,&msg->head);
	}
}

void ncServerClientReadCreateRoomMessage(Client *client, NetMsgT8 *msg)
{
	ChatRoom *rc = ncChatRoomCreate(client, msg->roomName);
	if (rc == NULL)
		ncServerSendClientTcpStandardMsg(client, 8, 0);
	else
		ncServerSendClientTcpStandardMsg(client, 8, rc->listItem.id);
}

void ncServerClientReadJoinRoomMessage(Client *client, NetMsg *msg)
{
	ChatRoom *chatRoom = ncChatRoomJoin(client, msg->msgId);
	if (chatRoom == NULL)
	{
		int rc = ncChatInfoJoin(client, msg->msgId);
		if (rc == 0)
			ncServerSendClientTcpStandardMsg(client, 10, 0);
	}
	else {
		ncServerSendClientTcpStandardMsg(client, 10, chatRoom->listItem.id);
		ncChatSendInfos(client, chatRoom->listItem.id);
	}
}

void ncServerClientReadChatMessage(Client *client, NetMsg *msg)
{
	if (client->status == CliChatting) {
		if (client->chatRoom != NULL)
			ncChatSendMsgExceptFrom(client->chatRoom, msg, client->listItem.id);
	}
	else if ((GameRoom *)client->chatRoom != NULL) {
		ncGameSendMsgExceptFrom((GameRoom *)client->chatRoom, msg, client->listItem.id);
	}
}

void ncServerClientReadCreateGameMessage(Client *client, NetMsgT18 *msg)
{
	GameRoom *gameRoom = ncGameRoomCreate(client, msg->gameName, msg->gameParams, msg->maxPlayers);
	if (gameRoom == NULL)
		ncServerSendClientTcpStandardMsg(client,18,0);
	else
		ncServerSendClientTcpStandardMsg(client,18,(gameRoom->listItem).id);
}

void ncServerClientReadJoinGameMessage(Client *client, NetMsg *msg)
{
	GameRoom *gameRoom = ncGameRoomJoin(client, msg->msgId);
	if (gameRoom == NULL) {
		ncServerSendClientTcpStandardMsg(client, 20, 0);
	}
	else {
		ncServerSendClientTcpStandardMsg(client, 20, gameRoom->listItem.id);
		ncGameSendInfos(client, gameRoom->listItem.id);
	}
}

void ncServerClientReadKickMessage(Client *client, NetMsg *msg)
{
	Client *kickedClient = ncClientFindById(msg->msgId);
	if (kickedClient != NULL)
		ncServerSendClientTcpStandardMsg(kickedClient, 38, client->listItem.id);
}

void ncServerClientReadMsgCallback(NetMsg *msg, Client *client)
{
	ncNbTcpReceived = ncNbTcpReceived + 1;
	if (client->status <= CliDisconnecting)
		return;

	client->tcpRecvMsg = client->tcpRecvMsg + 1;
	switch (msg->msgType)
	{
	case 2:
		ncServerClientReadPingMessage(client, (NetMsgT2 *)msg);
		break;
	case 7:
		ncServerClientReadClientInfosMessage(client, msg);
		break;
	case 8:
		ncServerClientReadCreateRoomMessage(client, (NetMsgT8 *)msg);
		break;
	case 10:
		ncServerClientReadJoinRoomMessage(client, msg);
		break;
	case 11:
		ncChatRoomQuit(client);
		break;
	case 14:
		ncServerClientReadChatMessage(client, msg);
		break;
	case 15:
		ncChatSendInfos(client, msg->msgId);
		break;
	case 16:
		ncChatSendList(client);
		break;
	case 18:
		ncServerClientReadCreateGameMessage(client, (NetMsgT18 *)msg);
		break;
	case 20:
		ncServerClientReadJoinGameMessage(client, msg);
		break;
	case 22:
		ncGameSendInfos(client, msg->msgId);
		break;
	case 24:
		ncGameRoomQuit(client);
		break;
	case 26:
		ncGameSendList(client);
		break;
	case 27:
		ncGameSetParams(client, (NetMsgT27 *)msg);
		break;
	case 28:
		ncGameSetPlayerParams(client, (NetMsgT28 *)msg);
		break;
	case 29:
		ncGameSetPlayerReady(client, msg);
		break;
	case 30:
		ncGameStartLoading(client, (NetMsgT30 *)msg);
		break;
	case 31:
		client->status = CliWaiting;
		break;
	case 34:
		ncGamePlayerResults(client, (NetMsgT34 *)msg);
		break;
	case 38:
		ncServerClientReadKickMessage(client,msg);
		break;
	case 42:
		memset(((GameRoom *)client->chatRoom)->pointsWon, 0, 4);
		break;
	}
}
