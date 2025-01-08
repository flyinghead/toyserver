#include "toyserver.h"
#include <errno.h>
#include <assert.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/select.h>

struct WaitingSocket
{
	struct Socket sock;
    time_t timer;
    int srcAddr;
    short tcpPort;
    short loginExpected;
};
typedef struct WaitingSocket WaitingSocket;

static WaitingSocket ncWaitingSockets[256];
static int ncNbWaitingSockets;
static int ncSocketListening = -1;

static int (*ncServerLoginCallback_)(int, uint32_t, uint16_t, uint8_t *data);

static fd_set read_fd_set;
static fd_set write_fd_set;
static int max_fd_set = -1;
struct SocketCallback
{
	void (*callback)(void *);
	void *argument;
};
struct SocketCallback readCallbacks[1024];
struct SocketCallback writeCallbacks[1024];

static void listeningSocketCallback(void *a);
static void waitingSocketCallback(void *arg);

static void udpSocketCallback(void *a) {
	ncGameManageUdp();
}

int ncServerStartListening(uint16_t tcpport)
{
	int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (fd == -1)
	{
		ncLogPrintf(0, "ERROR : ncServerStartListening() failed -> socket() error ");
		ncSocketError(ncSocketGetLastError());
		return 0;
	}
	ncLogPrintf(0, "Listening socket successfuly created = %d.", fd);
	int one = 1;
	int ret = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
	if (ret != 0) {
		ncLogPrintf(0, "ERROR : setsockopt(SO_REUSEADDR) failed: ");
		ncSocketError(ncSocketGetLastError());
	}
	struct sockaddr_in server_addr;
	server_addr.sin_family = 2;
	server_addr.sin_port = htons(tcpport);
	server_addr.sin_addr.s_addr = 0;
	ret = bind(fd, (struct sockaddr *)&server_addr, sizeof(server_addr));
	if (ret == -1)
	{
		ncLogPrintf(0,"ERROR : ncServerStartListening() failed -> bind() error (port %d) : ",
				tcpport);
		ncSocketError(ncSocketGetLastError());
		close(fd);
		return 0;
	}
	ncLogPrintf(0, "Listening socket successfully bound to port %d on any local IP.", tcpport);
	ret = listen(fd, 128);
	if (ret == -1)
	{
		ncLogPrintf(0,"ERROR : ncServerStartListening() failed -> listen() error : ");
		ncSocketError(ncSocketGetLastError());
		close(fd);
		return 0;
	}
	ncLogPrintf(0, "Socket is now listening...");
	ncNbWaitingSockets = 0;
	ncSocketListening = fd;
	asyncRead(fd, listeningSocketCallback, NULL);

	return 1;
}

void ncServerStopListening(void)
{
	ncSocketClose(&ncSocketListening);
	ncLogPrintf(0, "Listening Socket closed.");
	WaitingSocket *socket = &ncWaitingSockets[0];
	for (int i = 0; i < ncNbWaitingSockets; i++, socket++) {
		ncSocketTcpSendStandardMsg(socket->sock.fd, 5, 0);
		ncSocketClose(&socket->sock.fd);
	}
	ncNbWaitingSockets = 0;
}

static void listeningSocketCallback(void *a)
{
	struct sockaddr_in client_addr;
	socklen_t n = sizeof(client_addr);
	int sockfd = accept(ncSocketListening, (struct sockaddr *)&client_addr, &n);
	if (sockfd == -1)
	{
		ncLogPrintf(0, "ERROR : listeningSocketCallback()->accept() error ");
		ncSocketError(ncSocketGetLastError());
		return;
	}
	char *saddr = inet_ntoa(client_addr.sin_addr);
	if (ncNbWaitingSockets < 256)
	{
		if (ncSocketTcpSendStandardMsg(sockfd, 2, 0) == 0) {
			ncLogPrintf(0, "Connection ERROR from : %s , %d on socket %d",
					saddr, ntohs(client_addr.sin_port), sockfd);
			ncSocketClose(&sockfd);
		}
		else
		{
			WaitingSocket *socket = &ncWaitingSockets[ncNbWaitingSockets];
			ncNbWaitingSockets++;
			memset(socket, 0, sizeof(Socket));
			socket->sock.fd = sockfd;
			socket->timer = TimerRef;
			socket->loginExpected = 0;
			socket->tcpPort = client_addr.sin_port;
			socket->srcAddr = client_addr.sin_addr.s_addr;
			ncLogPrintf(0, "Connection established from : %s , %d on socket %d",
					saddr, ntohs(client_addr.sin_port), sockfd);
			asyncRead(sockfd, waitingSocketCallback, socket);
		}
	}
	else
	{
		ncSocketTcpSendStandardMsg(sockfd, 6, 0);
		ncSocketClose(&sockfd);
		ncLogPrintf(0, "!!! Too many waiting connections !!! : %s , %d is rejected", saddr, ntohs(client_addr.sin_port));
	}
}

void ncServerRemoveSocket(int idx)
{
	ncNbWaitingSockets--;
	for (int i = idx; i < ncNbWaitingSockets; i++)
		memcpy(&ncWaitingSockets[i], &ncWaitingSockets[i + 1], sizeof(WaitingSocket));
}

static void ncWaitingSocketMsgCallback(uint8_t *data, WaitingSocket *socket)
{
	if (socket == NULL)
		return;
	uint16_t msgType = *(uint16_t *)data;
	if (msgType == 1)
	{
		// Login
		if (socket->sock.fd != -1 && ncServerLoginCallback_ != NULL)
		{
			int ret = (*ncServerLoginCallback_)(socket->sock.fd, socket->srcAddr, socket->tcpPort, data);
			if (ret == 0)
				ncSocketClose(&socket->sock.fd);
			socket->sock.fd = -1;	// sockfd has been moved into the new Client
		}
	}
	else if (msgType == 2 && socket->sock.fd != -1)
	{
		// Ping
		time_t delta = TimerRef - socket->timer;
		if (delta <= 10000) {
			socket->loginExpected = 1;
			socket->timer = TimerRef;
		}
		else
		{
			ncSocketTcpSendStandardMsg(socket->sock.fd, 3, 0);
			ncLogPrintf(0, "!!! PING rejected = %d on socket %d.", (int)delta, socket->sock.fd);
			ncSocketClose(&socket->sock.fd);
		}
	}
	else {
		ncLogPrintf(0, "ERROR : bad message received on waiting socket %d !!!", socket->sock.fd);
		ncSocketClose(&socket->sock.fd);
	}
}

static void waitingSocketCallback(void *arg)
{
	WaitingSocket *socket = (WaitingSocket *)arg;
	int idx = socket - &ncWaitingSockets[0];
	ssize_t ret = ncSocketBufReadMsg(&socket->sock, (void (*)(uint8_t*, void*))ncWaitingSocketMsgCallback, socket);
	if (ret < 0)
	{
		ncSocketClose(&socket->sock.fd);
		ncServerRemoveSocket(idx);
		return;
	}
	if (ret == 0)
	{
		int timeout = 0;
		if (socket->loginExpected == 0)
		{
			if (TimerRef - socket->timer > 10000) {
				ncLogPrintf(0, "!!! waitingSocketCallback() PING timeout on socket %d.", socket->sock.fd);
				timeout = 1;
			}
		}
		else if (TimerRef - socket->timer > 12000) {
			ncLogPrintf(0, "!!! waitingSocketCallback() LOGIN timeout on socket %d.", socket->sock.fd);
			timeout = 1;
		}
		if (timeout) {
			ncSocketClose(&socket->sock.fd);
			ncServerRemoveSocket(idx);
		}
	}
}

void ncServerSetLoginCallback(void *callback) {
	ncServerLoginCallback_ = callback;
}

int ncServerSocketInit(uint16_t tcpPort, uint16_t udpPort)
{
	if (ncSocketInit() == 0)
		return 0;
	if (ncServerStartListening(tcpPort) == 0)
		return 0;

	ncServerUdpSocket = ncSocketUdpInit(udpPort);
	if (ncServerUdpSocket == -1)
		return 0;

	int bufsize = 0x8000;
	int ret = setsockopt(ncServerUdpSocket, SOL_SOCKET, SO_SNDBUF, &bufsize, sizeof(bufsize));
	if (ret == -1)
	{
		ncLogPrintf(0, "ERROR ncServerSocketInit()-> setsockopt( SO_SNDBUF ) failed : ");
		ncSocketError(ncSocketGetLastError());
		ncServerSocketRelease();
		return 0;
	}
	ret = setsockopt(ncServerUdpSocket, SOL_SOCKET, SO_RCVBUF, &bufsize, sizeof(bufsize));
	if (ret == -1)
	{
		ncLogPrintf(0, "ERROR ncServerSocketInit()-> setsockopt( SO_RCVBUF ) failed : ");
		ncSocketError(ncSocketGetLastError());
		ncServerSocketRelease();
		return 0;
	}
	ncServerUdpPort = udpPort;
	asyncRead(ncServerUdpSocket, udpSocketCallback, NULL);

	return 1;
}

void ncServerSocketRelease(void)
{
	ncSocketClose(&ncServerUdpSocket);
	ncServerStopListening();
	ncSocketRelease();
}

int ncSocketGetLastError(void) {
	return errno;
}

void ncSocketError(int error)
{
	switch(error) {
	case 4:
		ncLogPrintf(0, "A blocking Windows Socket 1.1 call was canceled through WSACancelBlockingCall.");
		break;
	default:
		ncLogPrintf(0, "Unknown error # %d.", error);
		break;
	case 9:
		ncLogPrintf(0, "For Windows CE AF_IRDA sockets only: the shared serial port is busy.");
		break;
	case 0xb:
		ncLogPrintf(0, "The socket is marked as nonblocking and no connections are present to be accepted.");
		break;
	case 0xe:
		ncLogPrintf(0, "Param error");
		break;
	case 0x16:
		ncLogPrintf(0, "Invalid parameters.");
		break;
	case 0x18:
		ncLogPrintf(0, "No more socket descriptors are available.");
		break;
	case 0x20:
		ncLogPrintf(0, "Broken pipe.");
		break;
	case 0x58:
		ncLogPrintf(0, "The descriptor is not a socket.");
		break;
	case 0x5a:
		ncLogPrintf(0, "The socket is message oriented, and the message is larger than the maximum supported by the underlying transport.");
		break;
	case 0x5b:
		ncLogPrintf(0, "The specified protocol (IPPROTO_TCP) is the wrong type for this socket.");
		break;
	case 0x5d:
		ncLogPrintf(0, "The specified protocol (IPPROTO_TCP) is not supported.");
		break;
	case 0x5e:
		ncLogPrintf(0, "The specified socket type (SOCK_STREAM) is not supported in this address family."
		);
		break;
	case 0x5f:
		ncLogPrintf(0, "Operation not supported by referenced socket.");
		break;
	case 0x61:
		ncLogPrintf(0, "The specified address family (AF_INET) is not supported.");
		break;
	case 0x62:
		ncLogPrintf(0, "A process on the machine is already bound to the same fully-qualified address and the socket has not been marked to allow address re-use ");
		break;
	case 99:
		ncLogPrintf(0, "The specified address is not a valid address for this machine.");
		break;
	case 100:
		ncLogPrintf(0, "The network subsystem or the associated service provider has failed.");
		break;
	case 0x65:
		ncLogPrintf(0, "The network cannot be reached from this host at this time.");
		break;
	case 0x66:
		ncLogPrintf(0, "The connection has been broken due to the keep-alive activity detecting a failure while the operation was in progress.");
		break;
	case 0x67:
		ncLogPrintf(0, "The virtual circuit was terminated due to a time-out or other failure. The application should close the socket as it is no longer usable.");
		break;
	case 0x68:
		ncLogPrintf(0, "Connection reset by peer.");
		break;
	case 0x69:
		ncLogPrintf(0, "Not enough buffers available or too many connections.");
		break;
	case 0x6a:
		ncLogPrintf(0, "The socket is already connected.");
		break;
	case 0x6b:
		ncLogPrintf(0, "Socket not connected");
		break;
	case 0x6c:
		ncLogPrintf(0, "The socket has been shut down.");
		break;
	case 0x6e:
		ncLogPrintf(0, "Timeout.");
		break;
	case 0x6f:
		ncLogPrintf(0, "The attempt to connect was forcefully rejected.");
		break;
	case 0x71:
		ncLogPrintf(0, "The remote host cannot be reached from this host at this time.");
		break;
	case 0x72:
		ncLogPrintf(0, "The socket is already connecting.");
		break;
	case 0x73:
		ncLogPrintf(0, "A blocking Windows Sockets 1.1 call is in progress, or the service provider is still processing a callback function.");
	}
}

int ncSocketInit(void)
{
	assert(sizeof(NetMsg) == 12);
	assert(sizeof(NetMsgT2) == 16);
	assert(sizeof(NetMsgT7) == 428);
	assert(sizeof(NetMsgT8) == 28);
	assert(sizeof(NetMsgT9) == 32);
	assert(sizeof(NetMsgT14) == 268);
	assert(sizeof(NetMsgT15) == 336);
	assert(sizeof(NetMsgT16) == 200);
	assert(sizeof(NetMsgT18) == 36);
	assert(sizeof(NetMsgT19) == 36);
	assert(sizeof(NetMsgT20) == 28);
	assert(sizeof(NetMsgT22) == 180);
	assert(sizeof(NetMsgT26) == 232);
	assert(sizeof(NetMsgT27) == 20);
	assert(sizeof(NetMsgT28) == 16);
	assert(sizeof(NetMsgT30) == 20);
	assert(sizeof(NetMsgT33) == 16);
	assert(sizeof(NetMsgT34) == 32);
	assert(sizeof(Result) == 16);
	return 1;
}

void ncSocketRelease(void) {
}

void ncSocketClose(int *pSockfd)
{
	if (*pSockfd == -1)
		return;
	cancelAsyncRead(*pSockfd);
	cancelAsyncWrite(*pSockfd);
	close(*pSockfd);
	ncLogPrintf(0, "Socket %d closed", *pSockfd);
	*pSockfd = -1;
}

uint8_t *ncSocketRead(int sockfd, ssize_t *len)
{
	static uint8_t ncReceiveBuff[1024];

	if (sockfd == -1)
	{
		if (len != NULL)
			*len = -1;
		return NULL;
	}
	ssize_t ret = recv(sockfd, ncReceiveBuff, sizeof(ncReceiveBuff), 0);
	if (ret > 0)
	{
		if (len != NULL)
			*len = ret;
		return ncReceiveBuff;
	}
	if (ret == -1)
	{
		ncLogPrintf(0, "ERROR : ncSocketRead()->recv(%d) error : ", sockfd);
		ncSocketError(ncSocketGetLastError());
		if (len != NULL)
			*len = -1;
	}
	else
	{
		ncLogPrintf(0, "!!! ncSocketRead()->recv(%d) 0 byte received : disconnection", sockfd);
		if (len != NULL)
			*len = -2;
	}
	return NULL;
}

static void socketSendCallback(void *arg)
{
	Socket *socket = (Socket *)arg;
	if (socket->sendBufSize > 0)
	{
		int rc = send(socket->fd, socket->sendBuf, socket->sendBufSize, MSG_NOSIGNAL);
		if (rc == -1)
		{
			ncLogPrintf(0,"ERROR : ncSocketTcpSend() failed -> send(%d) error ", socket->fd);
			ncSocketError(ncSocketGetLastError());
			cancelAsyncWrite(socket->fd);
			return;
		}
		if (rc == socket->sendBufSize)
			ncNbTcpSent++;;
		if (rc != 0)
			memmove(socket->sendBuf, socket->sendBuf + rc, socket->sendBufSize - rc);
		socket->sendBufSize -= rc;
	}
	if (socket->sendBufSize == 0)
		cancelAsyncWrite(socket->fd);
}

int ncSocketTcpSend(Socket *socket, void *data, size_t len)
{
	if (socket->fd == -1)
		return 0;
	if (len + socket->sendBufSize >= sizeof(socket->sendBuf)) {
		ncLogPrintf(0, "ERROR : ncSocketTcpSend() failed -> send buffer FULL !!!", socket->fd);
		return 0;
	}
	memcpy(socket->sendBuf + socket->sendBufSize, data, len);
	socket->sendBufSize += len;
	// TODO setting the socket no_delay would allow immediate send when possible
	asyncWrite(socket->fd, socketSendCallback, socket);
	return 1;
}

char *ncGetAddrString(in_addr_t addr)
{
	struct in_addr a = { addr };
	return inet_ntoa(a);
}

int ncSocketUdpInit(uint16_t port)
{
	int sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (sockfd == -1)
	{
		ncLogPrintf(0, "ERROR : ncSocketUdpInit() failed -> socket() error :");
		ncSocketError(ncSocketGetLastError());
		return -1;
	}
	ncLogPrintf(0, "UDP Socket (%d) successfuly created.", sockfd);
	struct sockaddr_in saddr;
	saddr.sin_family = 2;
	saddr.sin_port = htons(port);
	saddr.sin_addr.s_addr = 0;
	int rc = bind(sockfd, (struct sockaddr *)&saddr, sizeof(saddr));
	if (rc == -1)
	{
		ncLogPrintf(0, "ERROR : ncSocketUdpInit() failed -> bind(%d) error :", sockfd);
		ncSocketError(ncSocketGetLastError());
		ncSocketClose(&sockfd);
		return -1;
	}
	ncLogPrintf(0, "UDP Socket (%d) successfully bound to port %d on any local IP.",sockfd, port);
	return sockfd;
}

int ncSocketUdpSend(int sockfd, void *data, size_t len, in_addr_t dst_addr, uint16_t dstport)
{
	struct timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = 0;
	fd_set fdset;
	FD_ZERO(&fdset);
	FD_SET(sockfd, &fdset);
	int i = select(sockfd + 1, NULL, &fdset, NULL, &tv);	// TODO set socket to NDELAY instead
	if (i == -1)
	{
		ncLogPrintf(0, "ERROR : ncSocketUdpSend() failed -> select(%d) error ", sockfd);
		ncSocketError(ncSocketGetLastError());
		return 0;
	}
	if (i == 0) {
		ncLogPrintf(0, "ERROR : ncSocketUdpSend() failed -> select(%d) return 0 !!!", sockfd);
		return 0;
	}
	struct sockaddr_in dstaddr;
	memset(&dstaddr, 0, sizeof(dstaddr));
	dstaddr.sin_port = htons(dstport);
	dstaddr.sin_family = 2;
	dstaddr.sin_addr.s_addr = dst_addr;
	ssize_t sent = sendto(sockfd, data, len, 0, (struct sockaddr *)&dstaddr, sizeof(dstaddr));
	if (sent == len) {
		ncNbUdpSent++;
		return 1;
	}
	return 0;
}

void asyncRead(int fd, void(*callback)(void*), void *argument)
{
	FD_SET(fd, &read_fd_set);
	if (fd > max_fd_set)
		max_fd_set = fd;
	readCallbacks[fd].callback = callback;
	readCallbacks[fd].argument = argument;
}

void asyncWrite(int fd, void(*callback)(void*), void *argument)
{
	FD_SET(fd, &write_fd_set);
	if (fd > max_fd_set)
		max_fd_set = fd;
	writeCallbacks[fd].callback = callback;
	writeCallbacks[fd].argument = argument;
}

void cancelAsyncRead(int fd)
{
	FD_CLR(fd, &read_fd_set);
	readCallbacks[fd].callback = NULL;
}

void cancelAsyncWrite(int fd)
{
	FD_CLR(fd, &write_fd_set);
	writeCallbacks[fd].callback = NULL;
}

// returns -1 on error, 0 if timeout, >0 number of fds with event
int pollWait(int timeout)
{
	static fd_set rfd_set;
	static fd_set wfd_set;
	memcpy(&rfd_set, &read_fd_set, sizeof(rfd_set));
	memcpy(&wfd_set, &write_fd_set, sizeof(wfd_set));
	struct timeval tv;
	tv.tv_sec = timeout / 1000;
	tv.tv_usec = (timeout % 1000) * 1000;
	int ret = select(max_fd_set + 1, &rfd_set, &wfd_set, NULL, &tv);
	if (ret <= 0)
		return ret;
	for (int i = 0; i <= max_fd_set; i++)
	{
		if (FD_ISSET(i, &rfd_set) && readCallbacks[i].callback != NULL)
			readCallbacks[i].callback(readCallbacks[i].argument);
		if (FD_ISSET(i, &wfd_set) && writeCallbacks[i].callback != NULL)
			writeCallbacks[i].callback(writeCallbacks[i].argument);
	}
	return ret;
}
