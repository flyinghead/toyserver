SRC = ncList.c ncCRC32.c ncNetMsg.c ncServerChat.c ncServerClient.c ncServerClientMsg.c ncServerGame.c ncServerLog.c ncServerSocket.c toyserver.c
#CFLAGS = -g -Wall -fsanitize=address -static-libasan
CFLAGS = -O3 -Wall -DNDEBUG
INSTALL_DIR = /usr/local/toyserver
INSTALL_USER = toyserver

all: toyserver toyserver.service

toyserver: $(SRC) globals.h ncCRC32.h ncList.h netmsg.h toyserver.h
	$(CC) $(CFLAGS) -o $@ $(SRC)

clean:
	rm -f toyserver *.o toyserver.service

install: all
	install -o $(INSTALL_USER) -g $(INSTALL_USER) -d $(INSTALL_DIR)
	install --strip -o $(INSTALL_USER) -g $(INSTALL_USER) toyserver $(INSTALL_DIR)
	install -m 0644 -o $(INSTALL_USER) -g $(INSTALL_USER) ToyServer.cfg $(INSTALL_DIR)

toyserver.service: toyserver.service.in
	cp toyserver.service.in toyserver.service
	sed -e "s/INSTALL_USER/$(INSTALL_USER)/g" -e "s:INSTALL_DIR:$(INSTALL_DIR):g" < $< > $@

installservice: toyserver.service
	install -o root -g root -d /usr/local/lib/systemd/
	install -m 0644 -o root -g root $< /usr/local/lib/systemd/
	systemctl enable /usr/local/lib/systemd/toyserver.service
	