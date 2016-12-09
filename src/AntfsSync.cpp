
#include "AntfsSync.h"
#include "AntMessage.h"
#include "Storage.h"

#include <assert.h>
#include <iostream>
#include <iterator>
#include <fstream>
#include <iomanip>

namespace {

using namespace FitSync;

void DecodeBeaconMessage (const unsigned char *message, int /*size*/, std::ostream &out)
{
    unsigned char status1 = message[1];
    unsigned char status2 = message[2];
    //unsigned char auth_type = message[7];
    int desc1 = message[4] | (message[5] << 8);
    int desc2 = message[6] | (message[7] << 8);

    out << "BEACON(" << desc1 << "." << desc2 << "): ";
    out << "status ";
    switch(status2 & BEACON_STATE_MASK) {
    case BEACON_STATE_LINK:
        out << "LINK"; break;
    case BEACON_STATE_AUTH:
        out << "AUTH"; break;
    case BEACON_STATE_TRAN:
        out << "TRANSPORT"; break;
    case BEACON_STATE_BUSY:
        out << "BUSY"; break;
    default:
        out << "UNKNOWN - " << (int)(status2 & BEACON_STATE_MASK);
    }

    out << "; flags:";
    if (status1 | BEACON_DATA_AVAILABLE_FLAG)
        out << " DATA-AVAILABLE";
    if (status1 | BEACON_UPLOAD_ENABLED_FLAG)
        out << " UPLOAD-ENABLED";
    if (status1 | BEACON_PAIRING_ENABLED_FLAG)
        out << " PAIRING-ENABLED";

    out << "; channel period " << (int)(status1 & BEACON_CHANNEL_PERIOD_MASK) << std::endl;
}

// GCC does not seem to have put_time
std::string MyPutTime(const struct std::tm *tm, const char *format)
{
    char buf[1024];
    int r = std::strftime(buf, sizeof(buf), format, tm);
    if (r > 0)
        return std::string(&buf[0]);
    throw std::logic_error("strftime");
}

};                                      // end anonymous namespace

namespace FitSync {


// ........................................................... AntfsDirent ...

AntfsDirent::AntfsDirent (unsigned char *data, int /*size*/)
{
    m_Index = (data[0] | (data[1] << 8));
    m_Type = data[2];
    m_SubType = data[3];
    m_FileNum = (data[4] | (data[5] << 8));
    m_DataFlags = data[6];
    m_Flags = data[7];
    m_Size = (data[8] | (data[9] << 8) | (data[10] << 16) | (data[11] << 24));
    m_Timestamp = (data[12] | (data[13] << 8) | (data[14] << 16) | (data[15] << 24));
    // convert from FIT epoch (31 dec, 1989) to unix epoch
    m_Timestamp += 631065600;
}

std::string AntfsDirent::GetFileName() const
{
    struct std::tm tm = *localtime(&m_Timestamp);
    std::ostringstream o;
    o << MyPutTime(&tm, "%Y-%m-%d_%H-%M-%S")
      << "_" << static_cast<int>(m_SubType)
      << "_" << static_cast<int>(m_FileNum)
      << ".FIT";
    return o.str();
}


inline std::ostream& operator<< (std::ostream &o, const AntfsDirent &f)
{
    struct std::tm t = *localtime(&f.m_Timestamp);
    o << std::dec
      << f.m_Index << "\t" << f.m_Type << "\t" << f.m_SubType << "\t" << f.m_FileNum << "\t"
      << std::hex
      << (unsigned)f.m_DataFlags << "\t" << (unsigned)f.m_Flags << "\t"
      << std::dec
      << f.m_Size << "\t"
      << MyPutTime(&t, "%c");
    return o;
}


// ....................................................... AntfsChannel ....

AntfsChannel::AntfsChannel(AntStick *stick, int num, std::ostream *log_stream)
    : AntChannel (stick, num, BIDIRECTIONAL_RECEIVE, 4096, 0xff, 50),
      m_Retry(false),
      m_State (CH_EMPTY),
      m_NumSends(0),
      m_NumCompletedSends(0),
      m_NumTxFail(0),
      m_NumRxFail(0),
      m_LogStream(log_stream)
{
    if (m_LogStream == nullptr)
        m_LogStream = &std::cerr;
    ForgetDevice();
}
 
AntfsChannel::~AntfsChannel()
{
    // PutTimestamp(std::cout);
    // std::cout << "~AntfsChannel() -- sent " << m_NumSends << " packets, "
    //           << m_NumCompletedSends << " succesful, tx fail: "
    //           << m_NumTxFail << ", rx fail: " << m_NumRxFail << "\n";
}

void AntfsChannel::ProcessMessage (const unsigned char *data, int size)
{
    if (m_State == CH_CLOSED)
        return;
    
    if (data[2] == BROADCAST_DATA)
    {
        if (m_Retry) 
        {
            SendData(m_LastOutgoingMessage);
        }
        else if (data[4] == BEACON_ID)
        {
            OnBeacon(data + 4, size - 5); // strip off message header and trailing checksum
        }
    }
    else if (data[2] == RESPONSE_CHANNEL && data[4] == 1)
    {
        OnChannelEvent(static_cast<AntChannelEvent>(data[5]));
    }
    else if (data[2] == RESPONSE_CHANNEL
             && (data[4] == ACKNOWLEDGE_DATA 
                 || data[4] == BURST_TRANSFER_DATA))
    {
        OnAcknowledgeData(data, size);
    }
    else if (data[2] == BURST_TRANSFER_DATA)
    {
        // TODO: check sequence validity
        unsigned char seq = data[3] >> 5;
        if (seq == 0) {
            m_BurstPartialData.clear();
        }
        std::copy (data + 4, data + size - 1,
                   std::back_inserter (m_BurstPartialData));
        if (seq & 0x04)
            OnBurstTransfer (&m_BurstPartialData[0], m_BurstPartialData.size());
    }
    else
    {
        PutTimestamp(*m_LogStream);
        (*m_LogStream) << "AntChannel::ProcessMessage: received unknown packet\n";
        DumpData(data, size, *m_LogStream);
        (*m_LogStream) << std::flush;
    }
}

void AntfsChannel::SendData (const Buffer &data)
{
    // Non empty data and 8 byte padded.
    assert (! data.empty() && ((data.size() % 8) == 0));

    if(data.size() == 8) {
        // one single acknowledged packet
        auto m = MakeMessage (
            ACKNOWLEDGE_DATA, (unsigned char)m_ChannelNumber, data);
        m_Stick->WriteMessage (m);
    }
    else {
        // send burst transfer
        unsigned npackets = data.size() / 8;
        for (unsigned i = 0; i < npackets; ++i) {
            unsigned char seq = (i == 0) ? 0 : ((i - 1) % 3) + 1;
            if (i == npackets - 1) {
                seq |= 0x04;
            }
            Buffer pdata (&data[i * 8], &data[i * 8] + 8);
            unsigned char ch_seq = (seq << 5) | (unsigned char)(m_ChannelNumber);
            auto m = MakeMessage (BURST_TRANSFER_DATA, ch_seq, pdata);
            m_Stick->WriteMessage (m);
        }
    }

    m_LastOutgoingMessage = data;
    m_Retry = false;
    m_NumSends++;
}


void AntfsChannel::OnChannelEvent(AntChannelEvent e)
{
    switch (e) {
    case EVENT_TRANSFER_TX_COMPLETED:
        OnTxComplete();
        break;
    case EVENT_TRANSFER_TX_FAILED:
        OnTxFail();
        break;
    case EVENT_RX_FAIL:
    case EVENT_TRANSFER_RX_FAILED:
        OnRxFail();
        break;
    case EVENT_RX_FAIL_GO_TO_SEARCH:
        ForgetDevice();
        RequestClose();
        break;
    default:
        break;
    }
}

void AntfsChannel::OnTxComplete()
{
    m_NumCompletedSends++;
}

void AntfsChannel::OnTxFail()
{
    m_NumTxFail++;
    m_Retry = true;
}

void AntfsChannel::OnRxFail()
{
    // I believe the rx fail message indicates that the ANT stick failed to
    // receive some data from the watch, not much to do about that.
    m_Retry = true;
    m_NumRxFail++;
}

void AntfsChannel::OnAcknowledgeData (const unsigned char * /* data */, int /* size */)
{
    if (m_State == CH_LINK_REQ_SENT)
    {
        Configure(4096, 4, 19);
    }
}

void AntfsChannel::OnBurstTransfer (const unsigned char *data, int size)
{
    if (data[0] == BEACON_ID)
        OnBeacon (data, size);
    else
    {
        PutTimestamp(*m_LogStream);
        (*m_LogStream) << "Received unknown burst transfer" << std::endl;
        DumpData (data, size, *m_LogStream);
        (*m_LogStream) << std::flush;
    }
}

void AntfsChannel::OnCommand (const unsigned char *data, int size)
{
    AntfsCommands cmd = static_cast<AntfsCommands>(data[1]);

    switch (cmd) {
    case AUTHENTICATE_RESPONSE:
        OnAuthResponse(data, size);
        break;
    case DOWNLOAD_RESPONSE:
        OnDownloadResponse (data, size);
        break;
    default:
        PutTimestamp(*m_LogStream);
        (*m_LogStream) << "Unknown command: \n";
        DumpData (data, size, *m_LogStream);
        (*m_LogStream) << std::flush;
        break;
    }
}

void AntfsChannel::OnAuthResponse (const unsigned char *data, int size)
{
    if (size < 8)
        throw std::runtime_error ("OnAuthResponse -- short data");

    if (data[0] != ANTFS_HEADER && data[1] != AUTHENTICATE_RESPONSE)
        throw std::runtime_error ("OnAuthResponse -- bad header");

    AntAuthRespType type = static_cast<AntAuthRespType>(data[2]);
    unsigned char dlen = data[3];
    unsigned serial = data[4] | (data[5] << 8) | (data[6] << 16) | (data[7] << 24);

    // NOTE: there might be some padding in the packet, hence the less than test
    if (size < static_cast<int>(dlen + 7))
        throw std::runtime_error ("OnAuthResponse -- bad data length field value");

    // NOTE: 310XT will respond here with a 0 serial number, Garmin Swim will
    // return its actual serial.
    if(serial != 0 && m_DeviceSerial != 0 && serial != m_DeviceSerial)
    {
        std::ostringstream msg;
        msg << "OnAuthResponse -- received response from different serial: "
            << " got " << serial << ", expected " << m_DeviceSerial;
        throw std::runtime_error (msg.str());
    }

    switch (type) {
    case ARESP_NOT_AVAILABLE:
        if (m_State == CH_SERIAL_REQ_SENT) {
            // We requested the device serial and got it back
            m_DeviceSerial = serial;
	    const unsigned char *end = &data[8];
	    const unsigned char *max = &data[8] + dlen;
            while (*end != '\0' && end < max)
		end++;
            m_DeviceName = std::string (&data[8], end);

            time_t t; time(&t);
            time_t last_sync = GetLastSuccessfulSync(m_DeviceSerial);

            int seconds_since_sync = t - last_sync;
            // Keep 30 minutes between syncs
            bool recently_synched = (last_sync > 0 && (seconds_since_sync < 30 * 60));
            PutTimestamp(*m_LogStream);
            (*m_LogStream) << "Identified device " << m_DeviceName << " (" << m_DeviceSerial << ")";
            if (recently_synched) {
                (*m_LogStream) << ", recently synched (" << seconds_since_sync << " seconds ago)";
            }
            (*m_LogStream) << std::endl << std::flush;

            if (recently_synched) {
                RequestClose();
                m_State = CH_CLOSED;
            }
        }
        else {
            throw std::runtime_error ("OnAuthResponse (NOT_AVAILABLE) -- unexpected response");
        }
        break;
    case ARESP_ACCEPT:
        switch (m_State) {
        case CH_PAIR_REQ_SENT:
            // Client has accepted our pairing request, data contains
            // the key.
            PutTimestamp(*m_LogStream);
            (*m_LogStream) << "Device " << m_DeviceName << " (" << m_DeviceSerial
                           << ") accepted pairing request\n" << std::flush;
            PutKey (m_DeviceSerial, Buffer (&data[8], &data[8] + dlen));
            break;
        case CH_KEY_SENT:
            // Client has accepted our previously sent key.
            PutTimestamp(*m_LogStream);
            (*m_LogStream) << "Device " << m_DeviceName << " (" << m_DeviceSerial
                           << ") accepted key exchange\n" << std::flush;
            break;
        default:
            throw std::runtime_error ("OnAuthResponse (ACCEPT) -- unexpected response");
        }
        break;
    case ARESP_REJECT:
        if (m_State == CH_PAIR_REQ_SENT)
        {
            PutTimestamp(*m_LogStream);
            (*m_LogStream) << "Device " << m_DeviceName << " (" << m_DeviceSerial
                           << ") rejected pairing\n" << std::flush;
            m_State = CH_AUTH_REJECTED;
        }
        else if (m_State == CH_KEY_SENT)
        {
            PutTimestamp(*m_LogStream);
            (*m_LogStream) << "Device " << m_DeviceName << " (" << m_DeviceSerial
                           << ") rejected key\n" << std::flush;
            m_State = CH_AUTH_REJECTED;
        }
        else
        {
            throw std::runtime_error ("OnAuthResponse (REJECT) -- unexpected response");
        }
        break;
    default:
        PutTimestamp(*m_LogStream);
        (*m_LogStream) << "OnAuthResponse -- unknown type\n";
        DumpData (data, size, *m_LogStream);
        (*m_LogStream) << std::flush;
        break;
    }
}

void AntfsChannel::OnDownloadResponse (const unsigned char *data, int size)
{
    AntDownloadResponseType result = static_cast<AntDownloadResponseType>(data[2]);

    unsigned chunk = data[4] | (data[5] << 8) | (data[6] << 16) | (data[7] << 24);
    unsigned offset = data[8] | (data[9] << 8) | (data[10] << 16) | (data[11] << 24);
    unsigned total = data[12] | (data[13] << 8) | (data[14] << 16) | (data[15] << 24);

    unsigned crc_seed = data[size - 2] | (data[size - 1] << 8);

    m_DownloadResult = result;

    if (offset != m_Offset) {
        m_Retry = true;
	return;
    }

    bool download_complete = false;

    if (result == DRESP_OK)
    {
        std::copy(&data[16], &data[16] + chunk, std::back_inserter(m_FileData));
        m_Offset += chunk;
        m_CrcSeed = crc_seed;
        download_complete = (m_Offset == total);
    }
    else
    {
        // If there was an error, there is no point in continuing
        download_complete = true;
    }

    m_RequestNextChunk = ! download_complete;

    if (download_complete)
        OnDownloadComplete();
}

void AntfsChannel::OnBeacon(const unsigned char *data, int size)
{
    unsigned char status2 = data[2];
    int dev_state = (status2 & BEACON_STATE_MASK);

    switch (dev_state) {
    case BEACON_STATE_LINK:
        OnLinkBeacon (data, size);
        break;
    case BEACON_STATE_AUTH:
        OnAuthBeacon (data, size);
        break;
    case BEACON_STATE_BUSY:
        OnBusyBeacon (data, size);
        break;
    case BEACON_STATE_TRAN:
        OnTransportBeacon (data, size);
        break;
    default:
        PutTimestamp(*m_LogStream);
        (*m_LogStream) << "OnBeacon -- unknown beacon\n";
        DumpData (data, size, *m_LogStream);
        DecodeBeaconMessage(data, size, *m_LogStream);
        (*m_LogStream) << std::flush;
    }

    if (size > 8 && data[8] == ANTFS_HEADER)
    {
        // There's a command attached to this beacon
        OnCommand (data + 8, size - 8);
    }
}

void AntfsChannel::OnLinkBeacon (const unsigned char *data, int /*size*/)
{
    int device_id = data[4] | (data[5] << 8);
    int manufacturer_id = data[6] | (data[7] << 8);

    if (m_DeviceId == -1 && m_ManufacturerId == -1)
    {
        m_DeviceId = device_id;
        m_ManufacturerId = manufacturer_id;
        m_DeviceSerial = 0;
        PutTimestamp(*m_LogStream);
        if (IsBlackListed(manufacturer_id, device_id)) 
        {
            (*m_LogStream) << "Ignoring link request from blacklisted device "
                           << manufacturer_id << "." << device_id 
                           << std::endl << std::flush;
            RequestClose();
            m_State = CH_CLOSED;
            return;
        }
        else
        {
            (*m_LogStream) << "Received link request from "
                           << manufacturer_id << "." << device_id 
                           << std::endl << std::flush;
        }
    }
    else if (m_DeviceId != device_id && m_ManufacturerId != manufacturer_id)
    {
        // NOTE: should this be discarded silently?
        throw std::runtime_error ("OnLinkBeacon -- received link from another device");
    }

    SendData (MakeAntfsLinkRespose (19, 4, m_Stick->GetSerialNumber()));
    m_State = CH_LINK_REQ_SENT;
}

void AntfsChannel::OnAuthBeacon (const unsigned char *data, int /*size*/)
{
    unsigned our_serial = (data[4] | (data[5] << 8)
                           | (data[6] << 16) | (data[7] << 24));

    if (our_serial != m_Stick->GetSerialNumber())
    {
        // NOTE: does this mean the device tries to communicate with someone
        // else?
        throw std::runtime_error ("OnAuthBeacon -- bad serial");
    }

    if (m_DeviceId == -1 || m_ManufacturerId == -1)
    {
        // This is obtained during a LINK beacon, we didn't receive it.
        throw std::runtime_error ("OnAuthBeacon -- no device id");
    }

    if (m_DeviceSerial == 0)
    {
        if (m_State != CH_SERIAL_REQ_SENT)
            SendData (MakeAntfsAuthReq (AREQ_SERIAL, m_Stick->GetSerialNumber()));
        m_State = CH_SERIAL_REQ_SENT;
    }
    else
    {
        Buffer key = GetKey (m_DeviceSerial);

        if (IsBlackListed(m_DeviceSerial)) 
        {
            PutTimestamp(*m_LogStream);
            (*m_LogStream) << "Will not pair with blacklisted device "
                           << m_DeviceName << " (" << m_DeviceSerial << ")\n"
                           << std::flush;
            RequestClose();
            m_State = CH_CLOSED;
        }
        else if (key.empty())
        {
            if (m_State != CH_PAIR_REQ_SENT)
            {
                PutTimestamp(*m_LogStream);
                (*m_LogStream) << "Attempting pair request with "
                               << m_DeviceName << " (" << m_DeviceSerial << ")\n"
                               << std::flush;
                const char *n = "Antfs-Sync";
                const unsigned char *name = reinterpret_cast<const unsigned char*>(n);
                Buffer d (name, name + strlen(n) + 1);
                Buffer m = MakeAntfsAuthReq (AREQ_PAIRING, m_Stick->GetSerialNumber(), d);
                SendData (m);
                m_State = CH_PAIR_REQ_SENT;
            }
        }
        else
        {
            if (m_State != CH_KEY_SENT)
            {
                PutTimestamp(*m_LogStream);
                (*m_LogStream) << "Attempting key exchange with "
                               << m_DeviceName << " (" << m_DeviceSerial << ")\n"
                               << std::flush;
                Buffer m = MakeAntfsAuthReq (AREQ_PASSKEY_EXCHANGE, m_Stick->GetSerialNumber(), key);
                SendData (m);
                m_State = CH_KEY_SENT;
            }
        }
    }
}

void AntfsChannel::OnTransportBeacon (const unsigned char */*data*/, int /*size*/)
{
    if(m_FileIndex == -2)                 // special case, means close connection
    {
        PutTimestamp(*m_LogStream);
        (*m_LogStream) << "Disconnecting from "
                       << m_DeviceName << " (" << m_DeviceSerial << ")\n" << std::flush;
        Buffer m = MakeAntfsDisconnectReq (1, 0, 0);
        SendData (m);
        m_FileIndex = -3;
        return;
    }

    if (m_FileIndex == -1)                // this is the first time we see a transport beacon
    {
        PutTimestamp(*m_LogStream);
        (*m_LogStream) << "Downloading file index from "
                       << m_DeviceName << " (" << m_DeviceSerial << ")\n" << std::flush;
        m_FileIndex = 0;                  // the directory
        m_FileData.clear();
        m_Offset = 0;
        m_CrcSeed = 0;
        m_RequestNextChunk = true;
    }

    if (m_RequestNextChunk)
    {
        Buffer m = MakeAntfsDownloadRequest (m_FileIndex, m_Offset, true, m_CrcSeed);
        SendData (m);
        m_RequestNextChunk = false;
    }
}

void AntfsChannel::OnBusyBeacon (const unsigned char * /*data*/, int /*size*/)
{
    // nothing to do for now
}

void AntfsChannel::OnDownloadComplete()
{
    if (m_DownloadResult == DRESP_OK)
    {
        if (m_FileIndex == 0)
            OnDirectoryDownloadComplete();
        else
            OnFileDownloadComplete();
    }
    else
    {
        PutTimestamp(*m_LogStream);
        (*m_LogStream) << "Failed to download file index "
                       << m_FileIndex << " (code " << m_DownloadResult << ")\n"
                       << std::flush;
    }

    if (m_FileIndex > 0)
        m_DownloadBacklog.erase(m_DownloadBacklog.begin());

    ScheduleNextDownload();
}

void AntfsChannel::ScheduleNextDownload()
{
    if (m_DownloadBacklog.empty())
    {
        MarkSuccessfulSync(m_DeviceSerial);
        m_FileIndex = -2;
    }
    else
    {
        m_FileIndex = m_DownloadBacklog[0].Index();
        m_DownloadResult = DRESP_OK;
        m_FileData.clear();
        m_Offset = 0;
        m_CrcSeed = 0;
        m_RequestNextChunk = true;
    }
}


void AntfsChannel::OnDirectoryDownloadComplete()
{
//    unsigned version, struct_length, time_format;
//    unsigned current_system_time, last_modified;
//
//    version = m_FileData[0];
//    struct_length = m_FileData[1];
//    time_format = m_FileData[2];
//    current_system_time = (m_FileData[8] | (m_FileData[9] << 8)
//                           | (m_FileData[10] << 16) | (m_FileData[11] << 24));
//    last_modified = (m_FileData[12] | (m_FileData[13] << 8)
//                     | (m_FileData[14] << 16) | (m_FileData[15] << 24));
//
    // 31 dec 1989
    //const unsigned fit_epoch = 631065600;

    // time_t cst = static_cast<time_t>(current_system_time + fit_epoch);
    // time_t lm = static_cast<time_t>(last_modified + fit_epoch);

    // struct std::tm tmcst = *localtime(&cst);
    // struct std::tm tmlm = *localtime(&lm);

    // std::cout << "Directory version " << std::hex << version
    //           << std::dec << ", struct length: " << struct_length
    //           << ", time format: " << time_format
    //           << ", system time: "
    //           << std::put_time(&tmcst, "%c")
    //           << ", last modified: "
    //           << std::put_time(&tmlm, "%c")
    //           << "\n";

    std::vector<AntfsDirent> files;
    int nactivities = 0, activities_size = 0, total_size = 0;

    std::string dpath = GetDeviceStoragePath(m_DeviceSerial);
    std::ostringstream flist_path;
    flist_path << dpath << "/file_list.txt";
    std::ofstream flist_out(flist_path.str(), std::ios::trunc);

    int nfiles = (m_FileData.size() - 16) / 16;
    flist_out << "File list for " << m_DeviceName << " (" << m_DeviceSerial << ")\n"
              << "Index\tType\tSubType\tFileNum\tDflags\tFlags\tSize\tTimestamp\n";
    for (int i = 0; i < nfiles; ++i)
    {
        AntfsDirent f(&m_FileData[16 + i * 16], 16);

        total_size += f.Size();
        if(f.Type() == FT_FIT && f.SubType() == FST_ACTIVITY)
        {
            nactivities++;
            activities_size += f.Size();
        }

        if (f.Type() == FT_FIT && f.Readable())
        {
            // Check if we already have this file
            std::ostringstream p;
            p << GetFileStoragePath(m_DeviceSerial, f.SubType()) << '/' << f.GetFileName();
            std::ifstream i(p.str().c_str());
            if (! i) {                  // file does not exist
                files.push_back (f);
            }
        }

        flist_out << f << "\n";
    }

    int asz = activities_size / 1024;
    if ((activities_size % 1024) != 0) asz++;
    int tsz = total_size / 1024;
    if ((total_size / 1024) != 0) tsz++;

    flist_out << "Total of " << tsz << "k used (" << nactivities << " activities use " << asz << "k)\n";

    PutTimestamp(*m_LogStream);
    (*m_LogStream) << "Device " << m_DeviceName << " (" << m_DeviceSerial << ") has "
                   << tsz << "k used (" << nactivities << " activities use " << asz << "k)\n" << std::flush;

    m_DownloadBacklog.swap(files);

    if (m_DownloadBacklog.size() > 0)
    {

        int total_download = 0;
        for (unsigned i = 0; i < m_DownloadBacklog.size(); ++i)
            total_download += m_DownloadBacklog[i].Size();
        tsz = total_download / 1024;
        if ((total_download / 1024) != 0) tsz++;

        PutTimestamp(*m_LogStream);
        (*m_LogStream) << "Downloading " << m_DownloadBacklog.size() << " files, total of " << tsz << "k, from "
                       << m_DeviceName << " (" << m_DeviceSerial << ")\n" << std::flush;
    }
    else
    {
        PutTimestamp(*m_LogStream);
        (*m_LogStream) << "Nothing to download from " << m_DeviceName << " (" << m_DeviceSerial << ")\n" << std::flush;
    }
}


void AntfsChannel::OnFileDownloadComplete()
{
    assert(! m_DownloadBacklog.empty());
    assert(m_FileIndex == m_DownloadBacklog[0].Index());

    const AntfsDirent &f = m_DownloadBacklog[0];

    std::ostringstream p;
    p << GetFileStoragePath(m_DeviceSerial, f.SubType()) << '/' << f.GetFileName();

    try {
        WriteData(p.str().c_str(), m_FileData);
        PutTimestamp(*m_LogStream);
        (*m_LogStream) << "Wrote " << p.str() << ", " << m_FileData.size() << " bytes.\n" << std::flush;
    }
    catch (std::exception &e) {
        PutTimestamp(*m_LogStream);
        (*m_LogStream) << e.what() << "\n" << std::flush;
    }
}

void AntfsChannel::ForgetDevice()
{
    m_FileIndex = -1;
    m_DownloadResult = DRESP_OK;
    m_FileData.clear();
    m_Offset = 0;
    m_CrcSeed = 0;
    m_RequestNextChunk = false;
    m_DownloadBacklog.clear();
    m_BurstPartialData.clear();

    m_DeviceName.clear();
    m_DeviceSerial = 0;
    m_DeviceId = -1;
    m_ManufacturerId = -1;

    m_Retry = false;
}

};                                      // end namespace FitSync
