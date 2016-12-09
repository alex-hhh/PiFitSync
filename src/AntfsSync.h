#pragma once

#include "AntStick.h"
#include "AntMessage.h"
#include <string>
#include <ctime>
#include <iomanip>
#include <queue>

namespace FitSync {


// .......................................................... AntfsDirent ....

class AntfsDirent
{
public:
  
    AntfsDirent (unsigned char *data, int size);

    std::string GetFileName() const;

    int Index() const { return m_Index; }
    int Type() const { return m_Type; }
    AntfsFileSubType SubType() const { return static_cast<AntfsFileSubType>(m_SubType); }
    int Size() const { return m_Size; }
    int Readable() const { return m_Flags & FF_READ; }
  
    friend std::ostream& operator<< (std::ostream &o, const AntfsDirent &f);
  
private:
    int m_Index;
    int m_Type;
    int m_SubType;
    int m_FileNum;
    unsigned char m_DataFlags;
    unsigned char m_Flags;
    unsigned m_Size;
    std::time_t m_Timestamp;
};

std::ostream& operator<< (std::ostream &o, const AntfsDirent &f);


// ....................................................... AntfsChannel ....

class AntfsChannel : public AntChannel
{
public:

    AntfsChannel(AntStick *stick, int num, std::ostream *log_stream);
    ~AntfsChannel();

    void ProcessMessage (const unsigned char *data, int size);

private:


    enum ChannelState { CH_EMPTY, 
                        CH_SETUP, CH_LINK_REQ_SENT, CH_LINK_REQ_COMPLETE,
                        CH_SERIAL_REQ_SENT, 
                        CH_PAIR_REQ_SENT,
                        CH_KEY_SENT,
                        CH_AUTH_REJECTED,
                        CH_DOWNLOADING_FILE,

                        // The SYNC has been finished, all it remains is to
                        // close the channel.
                        CH_SYNC_FINISHED,

                        // The channel has been closed.  The only thing that
                        // can be done with this object is destroy it.
                        CH_CLOSED

    };

    void SendData (const Buffer &data);

    void OnCommand (const unsigned char *data, int size);
    void OnBurstTransfer (const unsigned char *data, int size);
    void OnAcknowledgeData (const unsigned char *data, int size);
    void OnChannelEvent(AntChannelEvent e);

    void OnTxComplete();
    void OnTxFail();
    void OnRxFail();

    void OnAuthResponse (const unsigned char *data, int size);
    void OnDownloadResponse (const unsigned char *data, int size);

    void OnBeacon(const unsigned char *data, int size);

    void OnLinkBeacon (const unsigned char *data, int size);
    void OnAuthBeacon (const unsigned char *data, int size);
    void OnTransportBeacon (const unsigned char *data, int size);
    void OnBusyBeacon (const unsigned char *data, int size);

    void OnDownloadComplete();

    void OnDirectoryDownloadComplete();
    void OnFileDownloadComplete();
    void ScheduleNextDownload();

    void ForgetDevice();

    bool m_Retry;
    Buffer m_LastOutgoingMessage;

    int m_FileIndex;                    // index of file currently downloading
    AntDownloadResponseType m_DownloadResult;
    Buffer m_FileData;
    unsigned m_Offset;
    unsigned m_CrcSeed;
    bool m_RequestNextChunk;

    std::vector<AntfsDirent> m_DownloadBacklog;

    Buffer m_BurstPartialData;

    ChannelState m_State;

    std::string m_DeviceName;
    unsigned m_DeviceSerial;
    int m_DeviceId;
    int m_ManufacturerId;

    // Staticstics
    int m_NumSends;
    int m_NumCompletedSends;
    int m_NumTxFail;
    int m_NumRxFail;

    std::ostream *m_LogStream;
};

};                                      // end namespace FitSync

/*
  Local Variables:
  mode: c++
  End:
*/
