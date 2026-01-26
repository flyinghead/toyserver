prefix = /usr/local
exec_prefix = $(prefix)
sbindir = $(exec_prefix)/sbin
sysconfdir = $(prefix)/etc
localstatedir = /var/local
user = toyserver
#CFLAGS = -g -Wall -fsanitize=address -static-libasan
CFLAGS = -O3 -g -Wall -DNDEBUG
CFLAGS += "-DCONFDIR=\"$(sysconfdir)\""
LIBS =
DCNET = 1

OBJS = ncList.o ncCRC32.o ncNetMsg.o ncServerChat.o ncServerClient.o ncServerClientMsg.o ncServerGame.o ncServerLog.o ncServerSocket.o toyserver.o
HEADERS = globals.h ncCRC32.h ncList.h netmsg.h toyserver.h

ifeq ($(DCNET),1)
  OBJS += dcnet.o
  LIBS += -ldcserver -Wl,-rpath,/usr/local/lib
  CFLAGS += -DDCNET
  user := dcnet
endif

all: toyserver

%.o: %.c $(HEADERS) Makefile
	$(CC) $(CFLAGS) -c -o $@ $<

toyserver: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LIBS)

clean:
	rm -f toyserver *.o toyserver.service

install: all
	mkdir -p $(DESTDIR)$(sbindir)
	install toyserver $(DESTDIR)$(sbindir)
	mkdir -p $(DESTDIR)$(sysconfdir)
	cp -n ToyServer.cfg $(DESTDIR)$(sysconfdir)

toyserver.service: toyserver.service.in Makefile
	cp toyserver.service.in toyserver.service
	sed -e "s/INSTALL_USER/$(user)/g" -e "s:LOCALSTATEDIR:$(localstatedir):g" -e "s:SBINDIR:$(sbindir):g" < $< > $@

installservice: toyserver.service
	mkdir -p $(localstatedir)/lib
	install -o $(user) -g $(user) -d $(localstatedir)/lib/toyserver
	mkdir -p $(localstatedir)/log
	cp $< /usr/lib/systemd/system/
	systemctl enable toyserver.service
