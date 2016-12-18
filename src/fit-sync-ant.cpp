#include "AntfsSync.h"
#include "Tools.h"
#include "Storage.h"

#include <sys/types.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <syslog.h>

#include <iostream>
#include <fstream>


using namespace FitSync;

unsigned char AntFsKey[] = {0xa8, 0xa4, 0x23, 0xb9, 0xf5, 0x5e, 0x63, 0xc1};


// ............................................................. daemon ....

/** NOTE: for this to work, we need to add the following to /etc/rc.local
    mkdir /run/antfs-tool
    chown pi /run/antfs-tool

    note that /run is on a tmpfs on raspberrypi
    
 */
const char *g_PidFile = "/run/fit-sync/fit-sync-ant.pid";



// ........................................................ application ....

/** Open channels on the AntStick s untill an exception is thrown. */
void ProcessChannels(AntStick &s, std::ostream &log)
{
    try {
	while (true) {
            // PutTimestamp(std::cout);
	    // std::cout << "Creating ANTFS channel 0...\n";
	    AntfsChannel c (&s, 0, &log);
	    while (c.IsOpen())
	    {
		s.Tick();
		struct timeval tv;
		tv.tv_sec = 0;
		tv.tv_usec = 1000;
		int r = libusb_handle_events_timeout_completed (nullptr, &tv, nullptr);
		if (r < 0)
		    throw LibusbException ("libusb_handle_events", r);
	    }
	}
    }
    catch (std::exception &e) {
        PutTimestamp(log);
	log << e.what() << std::endl;
    }
}

void ProcessAntSticks(std::ostream &log)
{
    while (true) {
        try {
            AntStick a;
            PutTimestamp(log);
            log << "USB Stick: Serial#: " << a.GetSerialNumber()
                << ", version " << a.GetVersion()
                << ", max " << a.GetMaxNetworks() << " networks, max "
                << a.GetMaxChannels() << " channels\n" << std::flush;
            a.SetNetworkKey (AntFsKey);
            ProcessChannels(a, log);
        }
        catch (const AntStickNotFound &e) {
            PutTimestamp(log);
            log << e.what() << std::endl;
            syslog(LOG_NOTICE, "will shutdown, could not find ANT stick device");
            return;
        }
        catch (std::exception &e) {
            PutTimestamp(log);
            log << e.what() << std::endl;
        }
    }
}

int main(int argc,  char** argv)
{
    bool daemon_mode = false;

    int opt = 0;
    while ((opt = getopt(argc, argv, "dh")) != -1) {
        switch (opt) {
        case 'd':
            daemon_mode = !daemon_mode;
            break;
        case 'h':
            std::cerr << "Usage: " << argv[0] << " [-d]\n";
            return 1;
            break;
        default:
            std::cerr << "Bad option: " << (char)opt << "\n";
            return 1;
            break;
        }
    }

    if (daemon_mode) {
        daemon(0, 0);
        openlog("fit-sync", 0, LOG_USER);
    }

    bool libusb_initialized = false;
    
    try {
        if (! AquirePidLock(g_PidFile)) return 1;
        int r = libusb_init(NULL);
        if (r < 0)
            throw LibusbException("libusb_init", r);
        libusb_initialized = true;
        if (daemon_mode)
        {
            std::ostringstream log_file;
            log_file << GetBaseStoragePath() << "/fit-sync-ant.log";
            std::ofstream log(log_file.str(), std::ios::app);
            if (log) {
                syslog(LOG_NOTICE, "started up, will use %s as the log file", log_file.str().c_str());
                ProcessAntSticks(log);
	    }
            else {
                return 1;
            }
        }
        else
        {
            ProcessAntSticks(std::cout);
        }
    }
    catch (const std::exception &e)
    {
        syslog(LOG_ERR, e.what());
        std::cerr << e.what() << std::endl;
    }
    ReleasePidLock(g_PidFile);
    if (libusb_initialized)
      libusb_exit(nullptr);
    if (daemon_mode)
        closelog();
    return 0;
}
