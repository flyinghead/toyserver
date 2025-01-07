SRC = ncList.c ncCRC32.c ncNetMsg.c ncServerChat.c ncServerClient.c ncServerClientMsg.c ncServerGame.c ncServerLog.c ncServerSocket.c toyserver.c
CFLAGS = -g -Wall -fsanitize=address -static-libasan

all: toyserver

toyserver: $(SRC) globals.h ncCRC32.h ncList.h netmsg.h toyserver.h
	$(CC) $(CFLAGS) -o $@ $(SRC)

clean:
	rm -f toyserver *.o

