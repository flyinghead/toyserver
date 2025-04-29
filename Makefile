# deps: libcurl-dev
SRC = ncList.c ncCRC32.c ncNetMsg.c ncServerChat.c ncServerClient.c ncServerClientMsg.c ncServerGame.c ncServerLog.c ncServerSocket.c toyserver.c discord.c
#CFLAGS = -g -Wall -fsanitize=address -static-libasan
CFLAGS = -O3 -g -Wall -DNDEBUG
INSTALL_DIR = /usr/local/toyserver
INSTALL_USER = toyserver

all: toyserver toyserver.service

toyserver: $(SRC) globals.h ncCRC32.h ncList.h netmsg.h toyserver.h Makefile
	$(CC) $(CFLAGS) -o $@ $(SRC) -lcurl -lpthread

clean:
	rm -f toyserver *.o toyserver.service

install: all
	install -o $(INSTALL_USER) -g $(INSTALL_USER) -d $(INSTALL_DIR)
	install --strip toyserver $(INSTALL_DIR)

toyserver.service: toyserver.service.in Makefile
	cp toyserver.service.in toyserver.service
	sed -e "s/INSTALL_USER/$(INSTALL_USER)/g" -e "s:INSTALL_DIR:$(INSTALL_DIR):g" < $< > $@

installservice: toyserver.service
	cp $< /usr/lib/systemd/system/
	systemctl enable /usr/lib/systemd/system/toyserver.service
