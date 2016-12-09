#pragma once

#include "AntMessage.h"
#include "LinuxUtil.h"                  // for the Buffer definition

namespace FitSync
{
    std::string GetBaseStoragePath();
    std::string GetDeviceStoragePath(unsigned device_serial);
    std::string GetFileStoragePath(unsigned device_serial, AntfsFileSubType t);

    void PutKey (unsigned device_serial, const Buffer &key);
    Buffer GetKey(unsigned device_serial);
    void RemoveKey(unsigned device_serial);

    void MarkSuccessfulSync(unsigned device_serial);
    time_t GetLastSuccessfulSync(unsigned device_serial);

    bool IsBlackListed(int manufacturer, int device);
    bool IsBlackListed(unsigned int device_serial);

}; // end namespace FitSync

/*
    Local Variables:
    mode: c++
    End:
*/
