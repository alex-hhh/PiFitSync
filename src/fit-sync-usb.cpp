#include "LinuxUtil.h"
#include "Storage.h"
#include "FitFile.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <strings.h>
#include <utime.h>
#include <queue>
#include <iostream>
#include <sstream>
#include <sstream>
#include <syslog.h>

using namespace FitSync;

/** NOTE: for this to work, we need to add the following to /etc/rc.local
    mkdir /run/antfs-tool
    chown pi /run/antfs-tool

    note that /run is on a tmpfs on raspberrypi

 */
const char *g_PidFile = "/run/fit-sync/fit-sync-usb.pid";

bool g_DaemonMode = false;

namespace {

struct FoundFitFileId {};

class MyBuilder : public fit::FitBuilder
{
public:

    MyBuilder(fit::FitFileId &fid) : m_fid(fid)
        {
        }

    void OnFitFileId(const fit::FitFileId &m) override
        {
            m_fid = m;
            throw FoundFitFileId ();
        }

private:
    fit::FitFileId &m_fid;
};


void GetFitFileId(Buffer &buf, fit::FitFileId &fid)
{
    MyBuilder b(fid);
    try {
        ReadFitMessages(buf, &b);
    }
    catch (const FoundFitFileId &) {
        // found it
    }
}

};                                      // end anonymous namespace

std::queue<std::string> DelayedDirs;

std::string BaseName(const std::string &path)
{
    auto p = path.find_last_of("/\\");
    if (p == std::string::npos)
        return "";
    else
        return path.substr(p + 1);
}


void ProcessFitFile(const std::string &path)
{
    try {
        Buffer fit_file;
        ReadData(path, fit_file);
        fit::FitFileId fid;
        GetFitFileId(fit_file, fid);
        auto p = GetFileStoragePath(
            fid.SerialNumber,
            static_cast<AntfsFileSubType>(fid.Type.value));
        std::ostringstream target;
        target << p << "/" << BaseName(path);
        WriteData(target.str(), fit_file);

        // Set the file access and modification times to the FIT creation
        // time, to make them easier to identify.
        struct utimbuf tb;
        tb.actime = tb.modtime = fid.TimeCreated;
        utime(target.str().c_str(), &tb);

        if (! g_DaemonMode)
            std::cout << path << " went into " << target.str() << "\n";
    }
    catch (const std::exception &e) {
        std::ostringstream msg;
        msg << path << ": " << e.what();
        if (g_DaemonMode)
            syslog(LOG_ERR, msg.str().c_str());
        else
            std::cerr << msg.str() << "\n";
    }
}

void ScanDir(const std::string &dir)
{
    DIR *d = opendir(dir.c_str());
    if (! d) {
        throw UnixException("opendir", errno);
    }
    while (struct dirent *e = readdir(d)) {
        std::ostringstream path;
        path << dir << "/" << e->d_name;
        struct stat buf;
        int r = stat(path.str().c_str(), &buf);
        if (r != 0) {
            auto ex = UnixException("stat", errno);
            std::ostringstream msg;
            msg << path.str() << ", " << ex.what();
            if (g_DaemonMode)
                syslog(LOG_ERR, msg.str().c_str());
            else
                std::cerr << msg.str() << "\n";
            continue;
        }
        if (S_ISDIR(buf.st_mode)) {
            // WARNING: funny, but correct test below
            if (strcmp(e->d_name, ".") && strcmp(e->d_name, ".."))
                DelayedDirs.push(path.str());
        }
        else if (S_ISREG(buf.st_mode)) {
            char *p = strrchr(e->d_name, '.');
            if (p && strcasecmp(p, ".fit") == 0)
                ProcessFitFile(path.str());
        }
    }
    closedir(d);
}


int main(int argc, char **argv)
{
    int opt = 0;
    while ((opt = getopt(argc, argv, "dh")) != -1) {
        switch (opt) {
        case 'd':
            g_DaemonMode = ! g_DaemonMode;
            break;
        case 'h':
            std::cerr << "Usage: " << argv[0] << " [-d] DIR\n";
            return 1;
            break;
        default:
            std::cerr << "Bad option: " << (char)opt << "\n";
            return 1;
            break;
        }
    }

    if (optind >= argc) {
        std::cerr << "Missing directory name\n";
        return 1;
    }

    char *dir = argv[optind];

    if (g_DaemonMode)
    {
        // switch to the work dir, so it is not unmounted from beneath us.
        chdir(dir);
        daemon(1, 0);
        openlog("fit-sync", 0, LOG_USER);
        syslog(LOG_NOTICE, "started up, will process %s", dir);
    }

    if (! AquirePidLock(g_PidFile)) return 1;

    try {
        DelayedDirs.push(dir);
        while(! DelayedDirs.empty()) {
            auto path = DelayedDirs.front();
            DelayedDirs.pop();
            ScanDir(path);
        }
    }
    catch (std::exception &e) {
        std::cout << e.what() << "\n";
        syslog(LOG_ERR, e.what());
    }

    ReleasePidLock(g_PidFile);

    if (g_DaemonMode)
    {
        syslog(LOG_NOTICE, "sync complete");
        closelog();
    }
}
