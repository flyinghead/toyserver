#include "toyserver.h"

// Type		Id		Size
// 1						ncServerLoginCallback
// 2		0		16		ncServerManageClients (TimerRef)
// 2		!=0		16		ping resp
// 3						ping rejected
// 4		0				login failed?
// 5		0				shutdown
// 6		0				login failed: server full
// 7				472?	ncServerClientReadClientInfosMessage, ReadClientInfosMessage *** different format? ***
// 8				28?		ncServerClientReadCreateRoomMessage
// 8		roomId			create chat room response
// 9		roomId	32		ncChatRoomCreate
// 10		roomId	28		ncChatRoomJoin, ncServerClientReadJoinRoomMessage
// 14		roomId	268		ncChatInfoJoin
// 15		0				ncChatSendInfos (null chat room)
// 15		roomId	336		ncChatSendInfos (!null chat room)
// 16		roomId	200		ncChatSendList
// 17		roomId			delete chat room
//
// 18		gameId	?		ncServerClientReadCreateGameMessage
// 18		gameId			create game resp
// 19		gameId	36		ncGameRoomCreate
// 20		gameId			ncGameRoomJoin, ncServerClientReadJoinGameMessage
// 21		gameId			ncGameRoomJoin
// 22		0				ncGameSendInfos (null game)
// 22		gameId	0xb4	ncGameSendInfos (!null game)
// 23		gameId			ncGameRoomQuit
// 24		userId			ncGameRoomQuit
// 25		gameId			delete game
// 26		0		232		ncGameSendList
// 27				20?		ncGameSetParams
// 28				13?		ncGameSetPlayerParams
// 30				20+?	ncGameStartLoading
// 32		raceId?			ncGameCheckLoad
// 33		gameId	16		ncGameStatusChanged
// 34				32		ncGamePlayerResults
// 35		raceId	20		UDP game update error (game not found)
// 36		raceId	116+?	UDP game update
// 37		raceId	20		UDP game update (dummy) for last racer
// 38		userId			kick user resp, ncGameStartLoading
// 39		byte[4]			race rankings [1-4]
// 40				268		broadcast message from server
// 41		version			login failed: wrong client version
// 42						?
// 43		byte[4]			points won for each player

uint32_t ComputeMsgCRC(NetMsg *data)
{
	uint32_t crc = ComputeCRC32((uint8_t *)data, 8);
	if (data->size > 12)
		crc = crc + ComputeCRC32((uint8_t *)data + sizeof(NetMsg), data->size - 12);
	return crc;
}

int ncSocketTcpSendStandardMsg(int sockfd, uint16_t msgType, int msgId)
{
	if (sockfd == -1)
		return 0;
	NetMsg msg;
	msg.msgType = msgType;
	msg.msgId = msgId;
	msg.size = 12;
	msg.crc = ComputeMsgCRC(&msg);
	int len = send(sockfd, &msg, msg.size, MSG_NOSIGNAL);
	if (len == -1)
	{
		int error = ncSocketGetLastError();
		ncLogPrintf(0, "ERROR : ncSocketTcpSendStandardMsg() failed -> send(%d) error ", sockfd);
		ncSocketError(error);
		return 0;
	}
	if (len == msg.size) {
		ncNbTcpSent = ncNbTcpSent + 1;
		return 1;
	}
	return 0;
}

int ncSocketTcpSendMsg(Socket *socket, NetMsg *msg) {
	msg->crc = ComputeMsgCRC(msg);
	return ncSocketTcpSend(socket, msg, msg->size);
}

int ncSocketUdpSendMsg(int sockfd, in_addr_t dstaddr, uint16_t dstport, NetMsg *msg) {
	msg->crc = ComputeMsgCRC(msg);
	return ncSocketUdpSend(sockfd,msg,msg->size,dstaddr,dstport);
}

ssize_t ncSocketReadMsg(int sockfd, void (*callback)(uint8_t *data, void *), void *cbArg)
{
	ssize_t len = 0;
	uint8_t *data = ncSocketRead(sockfd, &len);
	if (data != NULL)
	{
		uint8_t *dataStart = data;
		for (; data < dataStart + len; data = data + *(uint16_t *)(data + 2))
		{
			NetMsg *msg = (NetMsg *)data;
			uint32_t uVar1 = len - (data - dataStart);
			if (uVar1 >= 12 && uVar1 >= msg->size)
			{
				uint32_t crc = ComputeMsgCRC(msg);
				if (crc != msg->crc) {
					ncLogPrintf(0, "Bad CRC in paquet from socket %d, msg ID = %d, Msg type = %d",
							sockfd, msg->msgId, msg->msgType);
					return -3;
				}
				callback(data,cbArg);
			}
		}
	}
	return len;
}

ssize_t ncSocketBufReadMsg(Socket *socket, void (*callback)(uint8_t *data, void *), void *cbArg)
{
	uint8_t buf[sizeof(socket->recvBuf) * 2];

	ssize_t len = 0;
	uint8_t *data = ncSocketRead(socket->fd, &len);
	if (data != NULL && len != 0)
	{
		if (socket->recvBufSize + len > sizeof(buf) || len > sizeof(socket->recvBuf)) {
			ncLogPrintf(0, "Socket %d receive buffer full", socket->fd);
			return -2;
		}
		if (socket->recvBufSize != 0)
		{
			memcpy(buf, socket->recvBuf, socket->recvBufSize);
			memcpy(buf + socket->recvBufSize, data, len);
			data = buf;
			len += socket->recvBufSize;
			socket->recvBufSize = 0;
		}
		for (uint8_t *p = data; p < data + len; p += ((NetMsg *)p)->size)
		{
			NetMsg * msg = (NetMsg *)data;
			uint32_t size = len - (p - data);
			if (size < 12 || size < msg->size)
			{
				socket->recvBufSize = size;
				memcpy(socket->recvBuf, p, size);
				return len;
			}
			uint32_t crc = ComputeMsgCRC(msg);
			if (crc != msg->crc) {
				ncLogPrintf(0,"Bad CRC in paquet from socket %d, msg ID = %d, Msg type = %d",
						socket->fd, msg->msgId, msg->msgType);
				return -3;
			}
			callback(p,cbArg);
		}
	}
	return len;
}
