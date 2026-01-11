#CFLAGS = -g -Wall -fsanitize=address -static-libasan
CFLAGS = -O3 -g -Wall -DNDEBUG
INSTALL_DIR = /usr/local/toyserver
INSTALL_USER = toyserver
LIBS =
DCNET = 1

OBJS = ncList.o ncCRC32.o ncNetMsg.o ncServerChat.o ncServerClient.o ncServerClientMsg.o ncServerGame.o ncServerLog.o ncServerSocket.o toyserver.o
HEADERS = globals.h ncCRC32.h ncList.h netmsg.h toyserver.h

ifeq ($(DCNET),1)
  OBJS := $(OBJS) dcnet.o
  LIBS := $(LIBS) -ldcserver -Wl,-rpath,/usr/local/lib
  CFLAGS := $(CFLAGS) -DDCNET
  INSTALL_USER := dcnet
endif

all: toyserver toyserver.service

%.o: %.c $(HEADERS) Makefile
	$(CC) $(CFLAGS) -c -o $@ $<

toyserver: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LIBS)

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
