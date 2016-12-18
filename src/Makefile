#CXXFLAGS=-Wall -Wextra -g -std=c++1y -I/usr/local/include/libusb-1.0
CXXFLAGS=-Wall -Wextra -O2 -std=c++1y -I/usr/local/include/libusb-1.0
CXX=g++
LDFLAGS=-L/usr/local/lib -lusb-1.0 -lrt
INSTALL=install
SED=sed
PREFIX=/usr/local
# NOTE DESTDIR can be specified on the command line

#### non serviceable parts bellow

BINDIR=$(DESTDIR)$(PREFIX)/bin
UDEVDIR=$(DESTDIR)/etc/udev/rules.d
SYSTEMDDIR=$(DESTDIR)/etc/systemd/system

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
      fit-sync-epo.service

all : $(TARGETS)

fit-sync-ant : $(COMMON_OBJS) $(ANT_OBJS)
	@echo "Creating $@ ..."
	@$(CXX) $(LDFLAGS) -o $@ $^

fit-sync-usb : $(COMMON_OBJS) $(USB_OBJS)
	@echo "Creating $@ ..."
	@$(CXX) $(LDFLAGS) -o $@ $^

clean:
	-rm *.o *.d
	-rm fit-sync-ant fit-sync-usb 99-fit-sync.rules
	-rm fit-sync-usb.service fit-sync-epo.service

install: $(TARGETS)
	mkdir --parents $(BINDIR)
	$(INSTALL) --strip fit-sync-ant $(BINDIR)
	$(INSTALL) --strip fit-sync-usb $(BINDIR)
	$(INSTALL) -m 755 fit-sync-epo.py $(BINDIR)/fit-sync-epo
	mkdir --parents $(UDEVDIR)
	$(INSTALL) -m 644 99-fit-sync.rules $(UDEVDIR)
	$(INSTALL) -m 644 fit-sync-setup.service $(SYSTEMDDIR)
	$(INSTALL) -m 644 fit-sync-usb.service $(SYSTEMDDIR)
	$(INSTALL) -m 644 fit-sync-ant.service $(SYSTEMDDIR)
	$(INSTALL) -m 644 fit-sync-epo.service $(SYSTEMDDIR)
	systemctl daemon-reload
	systemctl enable fit-sync-setup.service fit-sync-ant.service

-include $(OBJS:.o=.d)

%.o : %.cpp
	@echo "Compiling $@ ..."
	@$(CXX) -c -MMD -MP $(CXXFLAGS) $*.cpp -o $*.o

% : %.in
	@echo "Creating $@ ..."
	@$(SED) s+@@PREFIX@@+$(PREFIX)+ $^ > $@
