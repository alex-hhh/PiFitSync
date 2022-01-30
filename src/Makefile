#CXXFLAGS=-Wall -Wextra -g -std=c++1y -I/usr/local/include/libusb-1.0
CXXFLAGS=-Wall -Wextra -O2 -std=c++17 -I/usr/include/libusb-1.0
CXX=g++
LDFLAGS=-lusb-1.0 -lrt
INSTALL=install
SED=sed
PREFIX=/usr/local
USER=$(shell whoami)
GROUP=$(firstword $(shell groups))

# NOTE DESTDIR can be specified on the command line

#### non serviceable parts bellow

BINDIR=$(PREFIX)/libexec/fit-sync
UDEVDIR=/etc/udev/rules.d
SYSTEMDDIR=/etc/systemd/system

COMMON_SOURCES=LinuxUtil.cpp Storage.cpp Tools.cpp AntMessage.cpp	\
		AntReadWrite.cpp AntStick.cpp AntfsSync.cpp FitFile.cpp
COMMON_OBJS=$(COMMON_SOURCES:.cpp=.o)

ANT_SOURCES=fit-sync-ant.cpp
ANT_OBJS=$(ANT_SOURCES:.cpp=.o)

USB_SOURCES=fit-sync-usb.cpp
USB_OBJS=$(USB_SOURCES:.cpp=.o)

TARGETS= fit-sync-ant fit-sync-usb fit-sync-epo.py \
      99-fit-sync.rules         \
      fit-sync-setup.service    \
      fit-sync-ant.service      \
      fit-sync-usb.service      \
      fit-sync-epo.service	\
      mount-fr945.service	\
      sync-fr945.service

all : $(TARGETS)

fit-sync-ant : $(COMMON_OBJS) $(ANT_OBJS)
	@echo "Creating $@ ..."
	@$(CXX) -o $@ $^ $(LDFLAGS)

fit-sync-usb : $(COMMON_OBJS) $(USB_OBJS)
	@echo "Creating $@ ..."
	@$(CXX) -o $@ $^ $(LDFLAGS)

clean:
	-rm *.o *.d
	-rm fit-sync-ant fit-sync-usb 99-fit-sync.rules
	-rm fit-sync-setup.service fit-sync-usb.service fit-sync-epo.service
	-rm fit-sync-ant.service
	-rm mount-fr945.service sync-fr945.service

install: $(TARGETS)
	mkdir --parents $(DESTDIR)$(BINDIR)
	mkdir --parents /media/garmin || true
	mkdir --parents /media/fr945 || true
	$(INSTALL) --strip fit-sync-ant $(DESTDIR)$(BINDIR)
	$(INSTALL) --strip fit-sync-usb $(DESTDIR)$(BINDIR)
	$(INSTALL) -m 755 fit-sync-epo.py $(DESTDIR)$(BINDIR)/fit-sync-epo
	$(INSTALL) -m 755 mount-fr945.sh $(DESTDIR)$(BINDIR)/mount-fr945
	$(INSTALL) -m 755 on-mtp-added.sh $(DESTDIR)$(BINDIR)/on-mtp-added
	$(INSTALL) -m 755 on-mtp-removed.sh $(DESTDIR)$(BINDIR)/on-mtp-removed
	mkdir --parents $(DESTDIR)$(UDEVDIR)
	$(INSTALL) -m 644 99-fit-sync.rules $(DESTDIR)$(UDEVDIR)
	$(INSTALL) -m 644 fit-sync-setup.service $(DESTDIR)$(SYSTEMDDIR)
	$(INSTALL) -m 644 fit-sync-usb.service $(DESTDIR)$(SYSTEMDDIR)
	$(INSTALL) -m 644 fit-sync-ant.service $(DESTDIR)$(SYSTEMDDIR)
	$(INSTALL) -m 644 fit-sync-epo.service $(DESTDIR)$(SYSTEMDDIR)
	$(INSTALL) -m 644 mount-fr945.service $(DESTDIR)$(SYSTEMDDIR)
	$(INSTALL) -m 644 sync-fr945.service $(DESTDIR)$(SYSTEMDDIR)
	systemctl daemon-reload
	systemctl enable fit-sync-setup.service
	udevadm control --reload-rules
	udevadm trigger --action=change # process any changes to the udev rules...

-include $(OBJS:.o=.d)

%.o : %.cpp
	@echo "Compiling $@ ..."
	@$(CXX) -c -MMD -MP $(CXXFLAGS) $*.cpp -o $*.o

% : %.in
	@echo "Creating $@ ..."
	@$(SED) -e "s+@@PREFIX@@+$(PREFIX)+" 	\
		-e "s+@@BINDIR@@+$(BINDIR)+" 	\
		-e "s+@@USER@@+$(USER)+"    	\
		-e "s+@@GROUP@@+$(GROUP)+"  	\
		$^ > $@
