
#define _CRT_SECURE_NO_WARNINGS

#include "AntMessage.h"
#include "Storage.h"

#include <string>
#include <vector>
#include <locale>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <map>

using namespace FitSync;

namespace {

    std::string g_BaseDirectory;

    const char *g_KeyFileName = "auth_key.dat";
    const char *g_AppName = "FitSync";

    std::map<unsigned, time_t> g_LastSuccessfulSync;

    // Map an FIT file type to a directory where we store it.
    struct FileTypeMap {
        AntfsFileSubType m_Type;
        const char *m_Name;
    } g_AntDirectory[] = {
        { FST_DEVICE, "." },
        { FST_SETTING, "Settings" },
        { FST_SPORT, "Sports" },
        { FST_MULTISPORT, "Sports" },
        { FST_ACTIVITY, "Activities" },
        { FST_WORKOUT, "Workouts" },
        { FST_COURSE, "Courses" },
        { FST_SCHEDULES, "Schedules" },
        { FST_WEIGHT, "Weight" },
        { FST_TOTALS, "Totals" },
        { FST_GOALS, "Goals" },
        { FST_BLOOD_PRESSURE, "Blood Pressure" },
        { FST_MONITORING_A, "Monitoring" },
        { FST_ACTIVITY_SUMMARY, "Activities" },
        { FST_MONITORING_DAILY, "Monitoring" },
        { FST_MONITORING_B, "Monitoring" } };

    const int g_NumAntDirectoryEntries = 
        sizeof(g_AntDirectory) / sizeof (g_AntDirectory[0]);

    void Init()
    {
        std::ostringstream path;
        path << GetUserDataDir() << '/' << g_AppName;
        g_BaseDirectory = path.str();
        MakeDirectoryPath(g_BaseDirectory);
    }

    std::string GetKeyFile(unsigned device_serial)
    {
        std::ostringstream fn;
        fn << GetDeviceStoragePath(device_serial) << '/' << g_KeyFileName;
        return fn.str();
    }

    const char* GetDirForFileType(AntfsFileSubType t)
    {
        for (int i = 0; i < g_NumAntDirectoryEntries; i++) {
            if (t == g_AntDirectory[i].m_Type)
                return g_AntDirectory[i].m_Name;
        }
        return "Unknown";
    }

};                                      // end anonymous namespace

namespace FitSync 
{

    std::string GetBaseStoragePath()
    {
        if (g_BaseDirectory == "") Init();
        return g_BaseDirectory;
    }

    std::string GetDeviceStoragePath(unsigned device_serial)
    {
        if (g_BaseDirectory == "") Init();
        std::ostringstream p;
        p << g_BaseDirectory << "/" << device_serial;
        std::string path = p.str();
        MakeDirectoryPath(path);
        return path;
    }

    std::string GetFileStoragePath(unsigned device_serial, AntfsFileSubType t)
    {
        if (g_BaseDirectory == "") Init();
        std::ostringstream p;
        p << g_BaseDirectory << '/' << device_serial << '/' << GetDirForFileType(t);
        std::string path = p.str();
        MakeDirectoryPath(path);
        return path;
    }

    void PutKey (unsigned device_serial, const Buffer &key)
    {
        if (! key.empty()) {
            WriteData(GetKeyFile(device_serial), key);
        }
    }

    Buffer GetKey(unsigned device_serial)
    {
        try {
            Buffer key;
            ReadData(GetKeyFile(device_serial), key);
            return key;
        }
        catch (...) {
            // TODO: maybe catch exceptions and only filter out "file not
            // exists" errors?
            return Buffer();
        }
    }

    void RemoveKey(unsigned device_serial)
    {
        RemoveFile(GetKeyFile(device_serial));
    }

    void MarkSuccessfulSync(unsigned device_serial)
    {
        time_t t;
        time(&t);
        g_LastSuccessfulSync[device_serial] = t;
    }

    time_t GetLastSuccessfulSync(unsigned device_serial)
    {
        // will create a 0 entry if device_serial has not been sync-ed yet,
        // which is fine
        return g_LastSuccessfulSync[device_serial];
    }

    bool IsBlackListed(int manufacturer, int device)
    {
        // Blacklist the Garmin Vector, as we don't have any activities to
        // download from it, yet it tries to connect, draining its battery.

        // NOTE: in the future, we might want to load the blacklist from a
        // file...  2161 is Vector_2
        return manufacturer == 1 && (device == 1381 || device == 2161);
    }

    bool IsBlackListed(unsigned int /*device_serial*/)
    {
        // NOTE: in the future, we might want to load the blacklist from a
        // file...
        return false;
    }


}; // end namespace FitSync
