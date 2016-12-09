#pragma once

/** Definitions for flags and value in different ANT message fields, plus
 * functions to construct messages. */

#include "Tools.h"

namespace FitSync {

    enum AntMessageId {
        SYNC_BYTE = 0xA4,
      
        INVALID = 0x00,

        // Configuration messages
        UNASSIGN_CHANNEL = 0x41,
        ASSIGN_CHANNEL = 0x42,
        SET_CHANNEL_ID = 0x51,
        SET_CHANNEL_PERIOD = 0x43,
        SET_CHANNEL_SEARCH_TIMEOUT = 0x44,
        SET_CHANNEL_RF_FREQ = 0x45,
        SET_NETWORK_KEY = 0x46,
        SET_TRANSMIT_POWER = 0x47,
        SET_SEARCH_WAVEFORM = 0x49, // XXX: Not in official docs
        ADD_CHANNEL_ID = 0x59,
        CONFIG_LIST = 0x5A,
        SET_CHANNEL_TX_POWER = 0x60,
        LOW_PRIORITY_CHANNEL_SEARCH_TIMOUT = 0x63,
        SERIAL_NUMBER_SET_CHANNEL = 0x65,
        ENABLE_EXT_RX_MESGS = 0x66,
        ENABLE_LED = 0x68,
        ENABLE_CRYSTAL = 0x6D,
        LIB_CONFIG = 0x6E,
        FREQUENCY_AGILITY = 0x70,
        PROXIMITY_SEARCH = 0x71,
        CHANNEL_SEARCH_PRIORITY = 0x75,
        // SET_USB_INFO                       = 0xff

        // Notifications
        STARTUP_MESSAGE = 0x6F,
        SERIAL_ERROR_MESSAGE = 0xAE,
      
        // Control messags
        RESET_SYSTEM = 0x4A,
        OPEN_CHANNEL = 0x4B,
        CLOSE_CHANNEL = 0x4C,
        OPEN_RX_SCAN_MODE = 0x5B,
        REQUEST_MESSAGE = 0x4D,
        SLEEP_MESSAGE = 0xC5,

        // Data messages
        BROADCAST_DATA = 0x4E,
        ACKNOWLEDGE_DATA = 0x4F,
        BURST_TRANSFER_DATA = 0x50,

        // Responses (from channel)
        RESPONSE_CHANNEL = 0x40,
        
        // Responses (from REQUEST_MESSAGE, 0x4d)
        RESPONSE_CHANNEL_STATUS = 0x52,
        RESPONSE_CHANNEL_ID = 0x51,
        RESPONSE_VERSION = 0x3E,
        RESPONSE_CAPABILITIES = 0x54,
        RESPONSE_SERIAL_NUMBER = 0x61
    };

    enum AntChannelType {
        BIDIRECTIONAL_RECEIVE = 0x00,
        BIDIRECTIONAL_TRANSMIT = 0x10,
    
        SHARED_BIDIRECTIONAL_RECEIVE = 0x20,
        SHARED_BIDIRECTIONAL_TRANSMIT = 0x30,
        
        UNIDIRECTIONAL_RECEIVE_ONLY = 0x40,
        UNIDIRECTIONAL_TRANSMIT_ONLY = 0x50
    };

    enum AntChannelEvent {
        EVENT_RX_SEARCH_TIMEOUT = 1,
        EVENT_RX_FAIL = 2,
        EVENT_TX = 3,
        EVENT_TRANSFER_RX_FAILED = 4,
        EVENT_TRANSFER_TX_COMPLETED = 5,
        EVENT_TRANSFER_TX_FAILED = 6,
        EVENT_CHANNEL_CLOSED = 7,
        EVENT_RX_FAIL_GO_TO_SEARCH = 8,
        EVENT_CHANNEL_COLLISION = 9,
        EVENT_TRANSFER_TX_START = 10
    };

    enum BeaconData {
        BEACON_ID = 0x43,
        BEACON_DATA_AVAILABLE_FLAG = 0x20,
        BEACON_UPLOAD_ENABLED_FLAG = 0x10,
        BEACON_PAIRING_ENABLED_FLAG = 0x08,

        BEACON_CHANNEL_PERIOD_MASK = 0x07,
        BEACON_STATE_MASK = 0x0f,

        BEACON_STATE_LINK = 0x00,
        BEACON_STATE_AUTH = 0x01,
        BEACON_STATE_TRAN = 0x02,
        BEACON_STATE_BUSY = 0x03
    };

    enum AntfsCommands {
        ANTFS_HEADER = 0x44,

        LINK = 0x02,
        DISCONNECT = 0x03,
        AUTHENTICATE = 0x04,
        PING = 0x05,

        DOWNLOAD_REQUEST = 0x09,
        UPLOAD_REQUEST = 0x0A,
        ERASE_REQUEST = 0x0B,
        UPLOAD_DATA = 0x0C,
        
        AUTHENTICATE_RESPONSE = 0x84,
        DOWNLOAD_RESPONSE = 0x89,
        UPLOAD_RESPONSE = 0x8A,
        ERASE_RESPONSE = 0x8B,
        UPLOAD_DATA_RESPONSE = 0x8C
    };

    enum AntAuthReqType {
        AREQ_PASS_THROUGH = 0,
        AREQ_SERIAL = 1,
        AREQ_PAIRING = 2,
        AREQ_PASSKEY_EXCHANGE = 3
    };

    enum AntAuthRespType {
        ARESP_NOT_AVAILABLE = 0,
        ARESP_ACCEPT = 1,
        ARESP_REJECT = 2
    };

    enum AntDownloadResponseType {
        DRESP_OK = 0,
        DRESP_NOT_FOUND = 1,
        DRESP_NOT_READABLE = 2,
        DRESP_NOT_READY = 3,
        DRESP_INVALID_REQUEST = 4,
        DRESP_BAD_CRC = 5
    };

    enum AntfsFileType {
        FT_FIT = 0x80
    };

    enum AntfsFileSubType {
        FST_DEVICE = 1,
        FST_SETTING = 2,
        FST_SPORT = 3,
        FST_ACTIVITY = 4,
        FST_WORKOUT = 5,
        FST_COURSE = 6,
        FST_SCHEDULES = 7,
        FST_WEIGHT = 9,
        FST_TOTALS = 10,
        FST_GOALS = 11,
        FST_BLOOD_PRESSURE = 14,
        FST_MONITORING_A = 15,
        FST_ACTIVITY_SUMMARY = 20,
        FST_MONITORING_DAILY = 28,
        FST_MONITORING_B = 32,
        FST_MULTISPORT = 33
    };

    enum AntfsFileFlags {
        FF_READ = 0x80,
        FF_WRITE = 0x40,
        FF_ERASE = 0x20,
        FF_ARCHIVED = 0x10,
        FF_APPEND_ONLY = 0x08,
        FF_ENCRYPTED = 0x40
    };

    bool IsGoodChecksum (const Buffer &message);

    Buffer MakeMessage (AntMessageId id, unsigned char d);
    Buffer MakeMessage (AntMessageId id, unsigned char d0, unsigned char d1);
    Buffer MakeMessage (AntMessageId id, unsigned char d0, unsigned char d1, unsigned char d2);
    Buffer MakeMessage (AntMessageId id, unsigned char d0, unsigned char d1, unsigned char d2, unsigned char d3, unsigned char d4);
    Buffer MakeMessage (AntMessageId id, Buffer data);
    Buffer MakeMessage (AntMessageId id, unsigned char data0, const Buffer &data);

    Buffer MakeAntfsLinkRespose (
        unsigned char frequency, unsigned char period, unsigned host_serial);
    Buffer MakeAntfsAuthReq (
        AntAuthReqType type, unsigned host_serial, const Buffer &data = Buffer());
    Buffer MakeAntfsDisconnectReq (
        unsigned char type, unsigned char duration, unsigned char app_duration);
    Buffer MakeAntfsDownloadRequest (
        unsigned file_index, unsigned offset, bool initial,
        unsigned crc_seed, unsigned max_block_size = 0);

};                                      // end namespace FitSync

/*
  Local Variables:
  mode: c++
  End:
*/
