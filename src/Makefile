#CXXFLAGS=-Wall -Wextra -g -std=c++1y -I/usr/local/include/libusb-1.0
CXXFLAGS=-Wall -Wextra -O2 -std=c++17 -I/usr/include/libusb-1.0
CXX=g++
LDFLAGS=-lusb-1.0 -lrt
INSTALL=install
SED=sed
PREFIX=/usr/local
USER=$(shell whoami)
GROUP=$(firstword $(shell groups))
HOME_DIR=$(HOME)

## Don't make the mount point the same as the volume label
## (e.g. /media/garmin), as systemd will attempt to automount it and will
## fail.
GARMIN_MOUNT_POINT=/media/garmin-edge

# NOTE DESTDIR can be specified on the command line

#### non serviceable parts bellow

BINDIR=$(PREFIX)/libexec/fit-sync
ETCDIR=$(PREFIX)/etc/fit-sync
UDEVDIR=/etc/udev/rules.d
SYSTEMDDIR=/etc/systemd/system

COMMON_SOURCES=LinuxUtil.cpp Storage.cpp Tools.cpp AntMessage.cpp	\
		AntReadWrite.cpp AntStick.cpp AntfsSync.cpp FitFile.cpp
COMMON_OBJS=$(COMMON_SOURCES:.cpp=.o)

ANT_SOURCES=fit-sync-ant.cpp
ANT_OBJS=$(ANT_SOURCES:.cpp=.o)

USB_SOURCES=fit-sync-usb.cpp
USB_OBJS=$(USB_SOURCES:.cpp=.o)

TARGETS= fit-sync-ant			\
	fit-sync-usb			\
	fit-sync-epo.py 		\
	99-fit-sync.rules 		\
	fit-sync-setup.service		\
	fit-sync-ant.service 		\
	fit-sync-usb.service 		\
	fit-sync-epo.service		\
	mount-garmin@.service 		\
	mount-fr945.service		\
	sync-fr945.service		\
	mount-edge540.service		\
	sync-edge540.service		\
	mount-edge840.service		\
	sync-edge840.service		\
	mount-bolt.service 		\
	sync-bolt.service 		\
	rwgps-sync-settings.json 	\
	rwgps-sync.service 		\
	rwgps-sync.timer

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
	-rm mount-edge540.service sync-edge540.service
	-rm mount-edge840.service sync-edge840.service
	-rm mount-bolt.service sync-bolt.service
	-rm rwgps-sync.service rwgps-sync-settings.json

install: $(TARGETS)
	mkdir --parents $(DESTDIR)$(BINDIR) || true
	mkdir --parents $(DESTDIR)$(UDEVDIR) || true
	mkdir --parents $(DESTDIR)$(ETCDIR) || true
	mkdir --parents /media/garmin-edge || true
	mkdir --parents /media/fr945 || true
	mkdir --parents /media/edge540 || true
	mkdir --parents /media/edge840 || true
	mkdir --parents /media/bolt || true
	$(INSTALL) --strip fit-sync-ant $(DESTDIR)$(BINDIR)
	$(INSTALL) --strip fit-sync-usb $(DESTDIR)$(BINDIR)
	$(INSTALL) -m 755 fit-sync-epo.py $(DESTDIR)$(BINDIR)/fit-sync-epo
	$(INSTALL) -m 755 rwgps-sync.py $(DESTDIR)$(BINDIR)/rwgps-sync
	# NOTE: everyone can read the settings file :-(
	$(INSTALL) -m 644 rwgps-sync-settings.json $(DESTDIR)$(ETCDIR)/
	$(INSTALL) -m 755 mount-fr945.sh $(DESTDIR)$(BINDIR)/mount-fr945
	$(INSTALL) -m 755 mount-edge540.sh $(DESTDIR)$(BINDIR)/mount-edge540
	$(INSTALL) -m 755 mount-edge840.sh $(DESTDIR)$(BINDIR)/mount-edge840
	$(INSTALL) -m 755 mount-bolt.sh $(DESTDIR)$(BINDIR)/mount-bolt
	$(INSTALL) -m 755 on-mtp-added.sh $(DESTDIR)$(BINDIR)/on-mtp-added
	$(INSTALL) -m 755 on-mtp-removed.sh $(DESTDIR)$(BINDIR)/on-mtp-removed
	$(INSTALL) -m 644 99-fit-sync.rules $(DESTDIR)$(UDEVDIR)
	$(INSTALL) -m 644 fit-sync-setup.service $(DESTDIR)$(SYSTEMDDIR)
	$(INSTALL) -m 644 fit-sync-usb.service $(DESTDIR)$(SYSTEMDDIR)
	$(INSTALL) -m 644 fit-sync-ant.service $(DESTDIR)$(SYSTEMDDIR)
	$(INSTALL) -m 644 fit-sync-epo.service $(DESTDIR)$(SYSTEMDDIR)
	$(INSTALL) -m 644 mount-garmin@.service $(DESTDIR)$(SYSTEMDDIR)
	$(INSTALL) -m 644 mount-fr945.service $(DESTDIR)$(SYSTEMDDIR)
	$(INSTALL) -m 644 sync-fr945.service $(DESTDIR)$(SYSTEMDDIR)
	$(INSTALL) -m 644 mount-edge540.service $(DESTDIR)$(SYSTEMDDIR)
	$(INSTALL) -m 644 sync-edge540.service $(DESTDIR)$(SYSTEMDDIR)
	$(INSTALL) -m 644 mount-edge840.service $(DESTDIR)$(SYSTEMDDIR)
	$(INSTALL) -m 644 sync-edge840.service $(DESTDIR)$(SYSTEMDDIR)
	$(INSTALL) -m 644 mount-bolt.service $(DESTDIR)$(SYSTEMDDIR)
	$(INSTALL) -m 644 sync-bolt.service $(DESTDIR)$(SYSTEMDDIR)
	$(INSTALL) -m 644 rwgps-sync.service $(DESTDIR)$(SYSTEMDDIR)
	$(INSTALL) -m 644 rwgps-sync.timer $(DESTDIR)$(SYSTEMDDIR)
	systemctl daemon-reload
	systemctl enable fit-sync-setup.service
	systemctl enable rwgps-sync.timer
	systemctl start rwgps-sync.timer
	# 25/06/11 Don't enable the fit-sync-ant service, as I have no devices
	# that use it systemctl enable fit-sync-ant.service
	udevadm control --reload-rules
	udevadm trigger --action=change # process any changes to the udev rules...

-include $(OBJS:.o=.d)

%.o : %.cpp
	@echo "Compiling $@ ..."
	@$(CXX) -c -MMD -MP $(CXXFLAGS) $*.cpp -o $*.o

% : %.in
	@echo "Creating $@ ..."
	@$(SED) -e "s+@@PREFIX@@+$(PREFIX)+"		\
		-e "s+@@BINDIR@@+$(BINDIR)+"		\
		-e "s+@@ETCDIR@@+$(ETCDIR)+"		\
		-e "s+@@HOME_DIR@@+$(HOME_DIR)+" 	\
		-e "s+@@USER@@+$(USER)+"		\
		-e "s+@@GROUP@@+$(GROUP)+"		\
		-e "s+@@GARMIN_MOUNT_POINT@@+$(GARMIN_MOUNT_POINT)+" \
		-e "s+@@RWGPS_SYNC_API_KEY@@+$(RWGPS_SYNC_API_KEY)+" \
		-e "s+@@RWGPS_SYNC_USER_EMAIL@@+$(RWGPS_SYNC_USER_EMAIL)+" 	\
		-e "s+@@RWGPS_SYNC_USER_PASSWORD@@+$(RWGPS_SYNC_USER_PASSWORD)+" \
		-e "s+@@RWGPS_HOME_DIR@@+$(HOME_DIR)+" 	\
		$^ > $@
