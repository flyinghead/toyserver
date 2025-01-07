#include "toyserver.h"
#include <time.h>

List ChatRooms;
ChatInfo InfoChats[50];
int NbInfoChat;

void (*ncChatRoomJoinCallback)(ChatRoom *);
void (*ncChatRoomQuitCallback)(Client *);
void (*ncChatRoomNewCallback)(ChatRoom *);

ChatRoom * ncServerGetChatById(int roomId)
{
	ChatRoom *chatRoom;

	chatRoom = (ChatRoom *)ncListFindById(&ChatRooms, roomId);
	return chatRoom;
}

int ncServerGetNbInfoChats(void) {
	return NbInfoChat;
}

ChatInfo * ncServerGetInfoChatIndexPtr(int chatIdx) {
	return InfoChats + chatIdx;
}

int ncServerCreateInfoChat(ChatInfo *srcChatInfo)
{
	if (NbInfoChat >= 50)
		return 0;

	ChatInfo *chatInfo = InfoChats + NbInfoChat;
	memcpy(chatInfo, srcChatInfo, sizeof(ChatInfo));
	int id = ncListGetNewID(&ChatRooms);
	chatInfo->roomId = id;
	srcChatInfo->roomId = id;
	NetMsgT9 msg;
	msg.head.msgId = chatInfo->roomId;
	msg.head.size = 32;
	msg.head.msgType = 9;
	msg.unk = 1;
	int i = NbInfoChat;
	msg.language = InfoChats[i].language;
	strcpy(msg.roomName, InfoChats[i].chatName);
	ncServerSendAllClientsTcpMsgExceptFrom(&msg.head, 0);
	NbInfoChat = NbInfoChat + 1;

	return 1;
}

void ncServerDeleteInfoChat(int roomId)
{
	ChatInfo *chatInfo = &InfoChats[0];
	int chatIdx = 0;
	do {
		if (chatIdx >= NbInfoChat)
		{
		LAB_0804d1a2: // FIXME
			for (; chatIdx < NbInfoChat; chatIdx++) {
				memcpy(chatInfo, chatInfo + 1, sizeof(ChatInfo));
				chatInfo++;
			}
			return;
		}
		if (chatInfo->roomId == roomId)
		{
			ncServerSendAllClientsStandardTcpMsg(17, chatInfo->roomId);
			NbInfoChat--;
			goto LAB_0804d1a2;
		}
		chatIdx++;
		chatInfo++;
	} while( 1 );
}

int ncChatInfoJoin(Client *client, int roomId)
{
	size_t len;
	NetMsgT14 msg;
	int i;
	ChatInfo *chatInfo;
	int chatIdx;

	chatInfo = InfoChats;
	chatIdx = 0;
	while( 1 )
	{
		if (NbInfoChat <= chatIdx)
			return 0;
		if (chatInfo->roomId == roomId)
			break;
		chatIdx = chatIdx + 1;
		chatInfo = chatInfo + 1;
	}
	memset(&msg, 0, sizeof(msg));
	msg.head.msgType = 14;
	for (i = 0; i < chatInfo->msgCount; i = i + 1)
	{
		strcpy(msg.message, chatInfo->messages[i]);
		len = strlen(msg.message);
		msg.head.size = ((short)len + 0x10) & 0xfffc;
		ncServerSendClientTcpMsg(client, &msg.head);
	}
	return 1;
}

int ncChatRoomAllocate(int maxChatRooms) {
	MaxNbChats = maxChatRooms;
	return ncListAllocate(&ChatRooms, maxChatRooms, sizeof(ChatRoom));
}

void ncChatRoomRelease(void) {
	ncListRelease(&ChatRooms);
}

ChatRoom *ncChatRoomCreate(Client *client, char *roomName)
{
	int ret;
	ChatRoom *chatRoom;
	NetMsgT9 msg;

	ret = ncListCheckNameUsed(&ChatRooms, roomName);
	if (ret == 0) {
		chatRoom = (ChatRoom *)ncListAdd(&ChatRooms, roomName);
		if (chatRoom == NULL) {
			ncLogPrintf(3, "ERROR creating new chat room.");
		}
		else {
			chatRoom->language = client->language;
			chatRoom->userCount = 1;
			time(&chatRoom->creationTime);
			chatRoom->clients[0] = client;
			client->chatRoom = chatRoom;
			client->status = CliChatting;
			const char *timestr = ncGetTimeString(chatRoom->creationTime);
			ncLogPrintf(3, "New chat room \'%s\' (ID=%d) created by \'%s\' (ID=%d) at %s.",
					chatRoom->listItem.name, chatRoom->listItem.id, client->listItem.name,
					client->listItem.id, timestr);
			if (ncChatRoomNewCallback != NULL)
				(*ncChatRoomNewCallback)(chatRoom);
			msg.head.msgId = chatRoom->listItem.id;
			msg.head.size = 32;
			msg.head.msgType = 9;
			msg.language = chatRoom->language;
			msg.unk = 0;
			strcpy(msg.roomName, chatRoom->listItem.name);
			ncServerSendAllClientsTcpMsgExceptFrom(&msg.head, client->listItem.id);
		}
	}
	else {
		chatRoom = NULL;
	}
	return chatRoom;
}

void ncChatSendMsgExceptFrom(ChatRoom *chatRoom, NetMsg *msg, int senderId)
{
	int idx;

	for (idx = 0; idx < (int)chatRoom->userCount; idx = idx + 1) {
		if (chatRoom->clients[idx] != NULL &&
				chatRoom->clients[idx]->listItem.id != senderId)
			ncServerSendClientTcpMsg(chatRoom->clients[idx], msg);
	}
}

void ncChatSendInfos(Client *client, int roomId)
{
	ChatRoom *chatRoom;
	int i;
	NetMsgT15 msg;

	chatRoom = (ChatRoom *)ncListFindById(&ChatRooms, roomId);
	if (chatRoom == NULL) {
		ncServerSendClientTcpStandardMsg(client, 0xf, 0);
	}
	else {
		msg.head.msgId = (chatRoom->listItem).id;
		msg.head.size = 336;
		msg.head.msgType = 15;
		msg.language = chatRoom->language;
		msg.userCount = chatRoom->userCount;
		for (i = 0; i < (int)chatRoom->userCount; i = i + 1) {
			strcpy(msg.userNames[i], chatRoom->clients[i]->listItem.name);
			msg.userIds[i] = chatRoom->clients[i]->listItem.id;
		}
		ncServerSendClientTcpMsg(client, &msg.head);
	}
}

ChatRoom * ncChatRoomJoin(Client *client, int roomId)
{
	NetMsgT20 msg10;
	NetMsg msg12;
	ChatRoom *chatRoom;

	chatRoom = (ChatRoom *)ncListFindById(&ChatRooms, roomId);
	if (chatRoom != NULL) {
		if (chatRoom->userCount < 16) {
			msg10.head.msgId = client->listItem.id;
			msg10.head.size = 28;
			msg10.head.msgType = 10;
			strcpy(msg10.playerName, client->listItem.name);
			ncChatSendMsgExceptFrom(chatRoom, &msg10.head, client->listItem.id);
			client->chatRoom = chatRoom;
			client->status = CliChatting;
			chatRoom->clients[chatRoom->userCount] = client;
			chatRoom->userCount = chatRoom->userCount + 1;
			msg12.msgId = chatRoom->listItem.id;
			msg12.size = 12;
			msg12.msgType = 12;
			ncServerSendAllClientsTcpMsgExceptFrom(&msg12, client->listItem.id);
			if (ncChatRoomJoinCallback != NULL)
				(*ncChatRoomJoinCallback)(chatRoom);
		}
		else {
			chatRoom = NULL;
		}
	}
	return chatRoom;
}

void ncChatRoomQuit(Client *client)
{
	if (client == NULL || client->chatRoom == NULL)
		return;
	ChatRoom *chatRoom = client->chatRoom;
	int idx;
	for (idx = 0; idx < (int)chatRoom->userCount; idx++)
	{
		if (client == chatRoom->clients[idx])
		{
			if (ncChatRoomQuitCallback != NULL)
				(*ncChatRoomQuitCallback)(client);
			NetMsg msg;
			msg.msgId = chatRoom->listItem.id;
			msg.msgType = 13;
			msg.size = 12;
			ncServerSendAllClientsTcpMsgExceptFrom(&msg, client->listItem.id);
			msg.msgId = client->listItem.id;
			msg.msgType = 11;
			ncChatSendMsgExceptFrom(chatRoom, &msg, client->listItem.id);
			client->chatRoom = NULL;
			if (client->status >= CliConnected)
				client->status = CliConnected;
			chatRoom->userCount -= 1;
			break;
		}
	}
	for (; idx < (int)chatRoom->userCount; idx = idx + 1) {
		chatRoom->clients[idx] = chatRoom->clients[idx + 1];
	}
}

void ncChatRoomSetNewCallback(void *callback) {
	ncChatRoomNewCallback = callback;
}

void ncChatRoomSetJoinCallback(void *callback) {
	ncChatRoomJoinCallback = callback;
}

void ncChatRoomSetQuitCallback(void *callback) {
	ncChatRoomQuitCallback = callback;
}

void ncChatRoomSetDeleteCallback(void *callback) {
	ChatRooms.delItemCallback = callback;
}

void ncChatRoomDelete(ChatRoom *room)
{
	int ret;

	ncServerSendAllClientsStandardTcpMsg(17, room->listItem.id);
	ncLogPrintf(3, "Chat room \'%s\' (ID=%d) deleted.", room->listItem.name, room->listItem.id);
	ret = ncListDelete(&ChatRooms, (ListItem *)room);
	if (ret == 0)
		ncLogPrintf(3, "ERROR deleting Chat room \'%s\' (ID=%d) deleted.", room->listItem.name, room->listItem.id);
}

void ncChatSendList(Client *client)
{
	int chatIdx;
	NetMsgT16 msg;
	ChatRoom *chatRoom;

	memset(&msg, 0, sizeof(msg));
	msg.head.msgId = 0;
	msg.head.size = 200;
	msg.head.msgType = 16;
	msg.count = 0;
	for (chatIdx = 0; chatIdx < NbInfoChat; chatIdx = chatIdx + 1)
	{
		if (7 < msg.count) {
			ncServerSendClientTcpMsg(client,&msg.head);
			msg.count = 0;
		}
		msg.chatIds[msg.count] = InfoChats[chatIdx].roomId;
		msg.languages[msg.count] = (InfoChats[chatIdx].language & 0xff) | 0x100;
		msg.userCount[msg.count] = 0;
		strcpy(msg.chatNames[msg.count], InfoChats[chatIdx].chatName);
		msg.count = msg.count + 1;
	}
	if (msg.count != 0) {
		ncServerSendClientTcpMsg(client, &msg.head);
		msg.count = 0;
	}
	for (chatRoom = (ChatRoom *)ChatRooms.head; chatRoom != NULL;
			chatRoom = (ChatRoom *)chatRoom->listItem.next) {
		if (7 < msg.count) {
			ncServerSendClientTcpMsg(client,&msg.head);
			msg.count = 0;
		}
		msg.chatIds[msg.count] = chatRoom->listItem.id;
		msg.languages[msg.count] = chatRoom->language & 0xff;
		msg.userCount[msg.count] = chatRoom->userCount;
		strcpy(msg.chatNames[msg.count], chatRoom->listItem.name);
		msg.count = msg.count + 1;
	}
	if (msg.count != 0)
		ncServerSendClientTcpMsg(client, &msg.head);
}

void ncChatRoomManage(void)
{
	int userIdx;
	ChatRoom *chatRoom;
	ChatRoom *nextChatRoom;

	chatRoom = (ChatRoom *)ChatRooms.head;
	nextChatRoom = chatRoom;
	do {
		chatRoom = nextChatRoom;
		if (chatRoom == NULL) {
			return;
		}
		nextChatRoom = (ChatRoom *)chatRoom->listItem.next;
		for (userIdx = 0; userIdx < (int)chatRoom->userCount; userIdx = userIdx + 1) {
			if (chatRoom->clients[userIdx]->status <= CliDisconnecting) {
				ncChatRoomQuit(chatRoom->clients[userIdx]);
				break;
			}
		}
		if (chatRoom->userCount == 0)
			ncChatRoomDelete(chatRoom);
	} while( 1 );
}

void ncServerEnumChatRooms(void (*callback)(ChatRoom *, void *), void *arg) {
	ncListEnum(&ChatRooms, (ListEnumCallback)callback, arg);
}
