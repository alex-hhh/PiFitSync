// Put include files outside the pragma section.

#include "AntMessage.h"
#include <algorithm>
#include <iterator>

using namespace FitSync;

namespace {

    void AddMessageChecksum (Buffer &b)
    {
        unsigned char c = 0;
        std::for_each (b.begin(), b.end(), [&](unsigned char e) { c ^= e; });
        b.push_back (c);
    }

    void PadData (Buffer &b)
    {
        int pad = b.size() % 8;
        if (pad > 0) {
            while (pad < 8) {
                b.push_back (0);
                pad++;
            }
        }
    }

};                                      // end anonymous namespace

namespace FitSync {

    bool IsGoodChecksum (const Buffer &message)
    {
        unsigned char c = 0;
        std::for_each (message.begin(), message.end(), [&](unsigned char e) { c ^= e; });
        return c == 0;
    }

    Buffer MakeMessage (AntMessageId id, unsigned char data)
    {
        Buffer b;
        b.push_back (SYNC_BYTE);
        b.push_back (0x01);                 // data length
        b.push_back (static_cast<unsigned char>(id));
        b.push_back (data);
        AddMessageChecksum (b);
        return b;
    }

    Buffer MakeMessage (AntMessageId id, unsigned char data0, unsigned char data1)
    {
        Buffer b;
        b.push_back (SYNC_BYTE);
        b.push_back (0x02);                 // data length
        b.push_back (static_cast<unsigned char>(id));
        b.push_back (data0);
        b.push_back (data1);
        AddMessageChecksum (b);
        return b;
    }

    Buffer MakeMessage (AntMessageId id, 
                        unsigned char data0, unsigned char data1, unsigned char data2)
    {
        Buffer b;
        b.push_back (SYNC_BYTE);
        b.push_back (0x03);                 // data length
        b.push_back (static_cast<unsigned char>(id));
        b.push_back (data0);
        b.push_back (data1);
        b.push_back (data2);
        AddMessageChecksum (b);
        return b;
    }

    Buffer MakeMessage (AntMessageId id, 
                        unsigned char data0, unsigned char data1, unsigned char data2, 
                        unsigned char data3, unsigned char data4)
    {
        Buffer b;
        b.push_back (SYNC_BYTE);
        b.push_back (0x05);                 // data length
        b.push_back (static_cast<unsigned char>(id));
        b.push_back (data0);
        b.push_back (data1);
        b.push_back (data2);
        b.push_back (data3);
        b.push_back (data4);
        AddMessageChecksum (b);
        return b;
    }

    Buffer MakeMessage (AntMessageId id, Buffer data)
    {
        Buffer b;
        b.push_back (SYNC_BYTE);
        b.push_back (static_cast<unsigned char>(data.size()));
        b.push_back (static_cast<unsigned char>(id));
        b.insert (b.end(), data.begin(), data.end());
        AddMessageChecksum (b);
        return b;
    }

    Buffer MakeMessage (AntMessageId id, unsigned char data0, const Buffer &data)
    {
        Buffer b;
        b.push_back (SYNC_BYTE);
        b.push_back (static_cast<unsigned char>(data.size() + 1));
        b.push_back (static_cast<unsigned char>(id));
        b.push_back (data0);
        b.insert (b.end(), data.begin(), data.end());
        AddMessageChecksum (b);
        return b;
    }

    Buffer MakeAntfsLinkRespose (
        unsigned char frequency, 
        unsigned char period, 
        unsigned host_serial)
    {
        Buffer b;
        b.push_back (ANTFS_HEADER);
        b.push_back (LINK);
        b.push_back (frequency);
        b.push_back (period);
        b.push_back (host_serial & 0xFF);
        b.push_back ((host_serial >> 8) & 0xFF);
        b.push_back ((host_serial >> 16) & 0xFF);
        b.push_back ((host_serial >> 24) & 0xFF);
        return b;
    }

    Buffer MakeAntfsAuthReq (
        AntAuthReqType type, 
        unsigned host_serial, 
        const Buffer &data)
    {
        Buffer b;
        b.push_back (ANTFS_HEADER);
        b.push_back (AUTHENTICATE);
        b.push_back(static_cast<unsigned char>(type));
        b.push_back (static_cast<unsigned char>(data.size()));
        b.push_back (host_serial & 0xFF);
        b.push_back ((host_serial >> 8) & 0xFF);
        b.push_back ((host_serial >> 16) & 0xFF);
        b.push_back ((host_serial >> 24) & 0xFF);
        std::copy (data.begin(), data.end(), std::back_inserter(b));
        PadData (b);
        return b;
    }

    Buffer MakeAntfsDisconnectReq (
        unsigned char type, 
        unsigned char duration, 
        unsigned char app_duration)
    {
        Buffer b;
        b.push_back (ANTFS_HEADER);
        b.push_back (DISCONNECT);
        b.push_back (type);
        b.push_back (duration);
        b.push_back (app_duration);
        PadData (b);
        return b;
    }

    Buffer MakeAntfsDownloadRequest (
        unsigned file_index,
        unsigned offset,
        bool initial,
        unsigned crc_seed,
        unsigned max_block_size)
    {
        Buffer b;
        b.push_back (ANTFS_HEADER);
        b.push_back (DOWNLOAD_REQUEST);
        b.push_back (file_index & 0xff);
        b.push_back ((file_index >> 8) & 0xff);
        b.push_back (offset & 0xff);
        b.push_back ((offset >> 8) & 0xff);
        b.push_back ((offset >> 16) & 0xff);
        b.push_back ((offset >> 24) & 0xff);
        b.push_back (0);                    // padding
        b.push_back (initial ? 1 : 0);
        b.push_back (crc_seed & 0xff);
        b.push_back ((crc_seed >> 8) & 0xff);
        b.push_back (max_block_size & 0xff);
        b.push_back ((max_block_size >> 8) & 0xff);
        b.push_back ((max_block_size >> 16) & 0xff);
        b.push_back ((max_block_size >> 24) & 0xff);
        PadData (b);
        return b;
    }


};                                      // end namespace FitSync

