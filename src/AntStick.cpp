#include "AntStick.h"
#include "AntMessage.h"
#include "AntReadWrite.h"
#include "Tools.h"

#include <iostream>
#include <algorithm>

#include <unistd.h>

namespace {


using namespace FitSync;

void CheckChannelResponse (
    const Buffer &response, unsigned char channel, unsigned char cmd, unsigned char status)
{
    if (response[2] != RESPONSE_CHANNEL
        || response[3] != channel
        || response[4] != cmd
        || response[5] != status)
    {
	DumpData(&response[0], response.size(), std::cerr);
	std::cout << "expecting channel: " << channel << ", cmd  " << (int)cmd << ", statis " << (int)status << "\n";
        throw std::runtime_error ("CheckChannelResponse -- bad response");
    }
}

bool SetAsideMessage(const Buffer &message)
{
    return (message[2] == BROADCAST_DATA
            || message[2] == BURST_TRANSFER_DATA
            || (message[2] == RESPONSE_CHANNEL
                && (message[4] == 0x01 || message[4] == ACKNOWLEDGE_DATA || message[4] == BURST_TRANSFER_DATA)));
}

};                                      // end anonymous namespace


namespace FitSync {


// ......................................................... AntChannel ....

AntChannel::AntChannel (AntStick *stick, int num, AntChannelType type, 
                        unsigned period, unsigned char timeout, unsigned char frequency)
    : m_IsOpen(false),
      m_ChannelNumber (static_cast<unsigned char>(num)),
      m_Stick (stick)
{
    m_Stick->WriteMessage (
        MakeMessage (
            ASSIGN_CHANNEL, m_ChannelNumber, 
            static_cast<unsigned char>(type),
            static_cast<unsigned char>(m_Stick->GetNetwork())));
    Buffer response = m_Stick->ReadMessage();
    CheckChannelResponse (response, m_ChannelNumber, ASSIGN_CHANNEL, 0);

    m_Stick->WriteMessage (
        MakeMessage (SET_CHANNEL_ID, m_ChannelNumber, 0x00, 0x00, 0x01, 0x00));
    response = m_Stick->ReadMessage();
    CheckChannelResponse (response, m_ChannelNumber, SET_CHANNEL_ID, 0);

    m_Stick->WriteMessage (
        MakeMessage (SET_SEARCH_WAVEFORM, m_ChannelNumber, 0x53, 0x00));
    response = m_Stick->ReadMessage();
    CheckChannelResponse (response, m_ChannelNumber, SET_SEARCH_WAVEFORM, 0);

    Configure(period, timeout, frequency);

    m_Stick->WriteMessage (
        MakeMessage (OPEN_CHANNEL, m_ChannelNumber));
    response = m_Stick->ReadMessage();
    CheckChannelResponse (response, m_ChannelNumber, OPEN_CHANNEL, 0);

    m_IsOpen = true;
  
    m_Stick->RegisterChannel (this);
}

AntChannel::~AntChannel()
{
    try {
        // The user has not called RequestClose(), try to close the channel
        // now, but this might fail.
        if (m_IsOpen) {
            m_Stick->WriteMessage (MakeMessage (CLOSE_CHANNEL, m_ChannelNumber));
            Buffer response = m_Stick->ReadMessage();
            CheckChannelResponse (response, m_ChannelNumber, CLOSE_CHANNEL, 0);

            // The channel has to respond with an EVENT_CHANNEL_CLOSED channel
            // event, but we cannot process that (it would go through
            // Tick(). We wait at least for the event to be generated.
            sleep(1);
          
            m_Stick->WriteMessage (MakeMessage (UNASSIGN_CHANNEL, m_ChannelNumber));
            response = m_Stick->ReadMessage();
            CheckChannelResponse (response, m_ChannelNumber, UNASSIGN_CHANNEL, 0);
        }
    }
    catch (std::exception &) {
        // discard it
    }

    m_Stick->UnregisterChannel (this);
}

void AntChannel::Configure (unsigned period, unsigned char timeout, unsigned char frequency)
{
    m_Stick->WriteMessage (
        MakeMessage (SET_CHANNEL_PERIOD, m_ChannelNumber, period & 0xFF, (period >> 8) & 0xff));
    Buffer response = m_Stick->ReadMessage();
    CheckChannelResponse (response, m_ChannelNumber, SET_CHANNEL_PERIOD, 0);

    m_Stick->WriteMessage (
        MakeMessage (SET_CHANNEL_SEARCH_TIMEOUT, m_ChannelNumber, timeout));
    response = m_Stick->ReadMessage();
    CheckChannelResponse (response, m_ChannelNumber, SET_CHANNEL_SEARCH_TIMEOUT, 0);

    m_Stick->WriteMessage (
        MakeMessage (SET_CHANNEL_RF_FREQ, m_ChannelNumber, frequency));
    response = m_Stick->ReadMessage();
    CheckChannelResponse(response, m_ChannelNumber, SET_CHANNEL_RF_FREQ, 0);
}

void AntChannel::HandleMessage(const unsigned char *data, int size)
{
    if (! m_IsOpen)
    {
        // We should not receive messages if we are closed, maybe we should
        // throw?
        std::cerr << "AntChannel::HandleMessage -- received a message while closed\n";
        DumpData(data, size, std::cerr);
        return;
    }
    
    if (data[2] == RESPONSE_CHANNEL && data[4] == 1)
    {
        AntChannelEvent e = static_cast<AntChannelEvent>(data[5]);
        if (e == EVENT_CHANNEL_CLOSED)
        {
            m_IsOpen = false;
            m_Stick->WriteMessage (MakeMessage (UNASSIGN_CHANNEL, m_ChannelNumber));
            Buffer response = m_Stick->ReadMessage();
            CheckChannelResponse (response, m_ChannelNumber, UNASSIGN_CHANNEL, 0);
            return;
        }
    }

    // Pass on the mesage to the derived class if we did not handle it.
    ProcessMessage(data, size);
}

void AntChannel::RequestClose()
{
    m_Stick->WriteMessage (MakeMessage (CLOSE_CHANNEL, m_ChannelNumber));
    Buffer response = m_Stick->ReadMessage();
    CheckChannelResponse (response, m_ChannelNumber, CLOSE_CHANNEL, 0);
}



// ........................................................... AntStick ....

const char * AntStickNotFound::what() const noexcept
{
    return "USB ANT stick not found";
}

// ANT+ memory sticks vendor and product ids.  We will use the first USB
// device found.
struct ant_stick_devid_ {
    int vid;
    int pid;
} ant_stick_devid[] = {
    {0x0fcf, 0x1008},
    {0x0fcf, 0x1009}
};

int num_ant_stick_devid = sizeof(ant_stick_devid) / sizeof(ant_stick_devid[0]);

/** Find the USB device for the ANT stick.  Return nullptr if not found,
 * throws an exception if there is a problem with the lookup.
 */
libusb_device* FindAntStick()
{
    libusb_device **devs;
    ssize_t devcnt = libusb_get_device_list(nullptr, &devs);
    if (devcnt < 0)
        throw LibusbException("libusb_get_device_list", devcnt);

    libusb_device *ant_stick = nullptr; // the one we are looking for
    bool found_it = false;
    int i = 0;
    libusb_device *dev = nullptr;

    while ((dev = devs[i++]) != nullptr && ! found_it)
    {
        struct libusb_device_descriptor desc;
        int r = libusb_get_device_descriptor(dev, &desc);
        if (r < 0) {
            libusb_free_device_list(devs, 1);
            throw LibusbException("libusb_get_device_descriptor", r);
        }

        for (int i = 0; i < num_ant_stick_devid; ++i)
        {
            if (desc.idVendor == ant_stick_devid[i].vid
                && desc.idProduct == ant_stick_devid[i].pid)
            {
                ant_stick = dev;
                libusb_ref_device(ant_stick);
                found_it = true;
                break;
            }
        }
    }
    libusb_free_device_list(devs, 1);

    return ant_stick;
}

/** Perform USB setup stuff to get the USB device ready for communication.
 */
void ConfigureAntStick(libusb_device_handle *ant_stick)
{
    // Detach any kernel driver from the interface we are about to claim.
    // This API call is Linux only.
    int r = libusb_detach_kernel_driver(ant_stick, 0);
    if (r != 0 && r != LIBUSB_ERROR_NOT_FOUND)
        throw LibusbException("libusb_detach_kernel_driver", r);
    
    r = libusb_claim_interface(ant_stick, 0); // Interface 0 must always exist
    if (r < 0)
        throw LibusbException("libusb_claim_interface", r);

    int actual_config;
    int desired_config = 1; // ant sticks support only one configuration
    r = libusb_get_configuration(ant_stick, &actual_config);
    if (r < 0)
        throw LibusbException("libusb_get_configuration", r);

    if (actual_config != desired_config)
    {
        r = libusb_set_configuration(ant_stick, desired_config);
        if (r < 0)
            throw LibusbException("libusb_set_configuration", r);
    }
    r = libusb_reset_device(ant_stick);
    if (r < 0)
        throw LibusbException("libusb_reset_device", r);
}

/** Return the read and write end USB endpoints for the ANT stick device.
 * These will be used to read/write data from/to the ANT stick.
 */
void GetAntStickReadWriteEndpoints(
    libusb_device *ant_stick,
    unsigned char *read_endpoint, unsigned char *write_endpoint)
{
    libusb_config_descriptor *cdesc = nullptr;
    int r = libusb_get_config_descriptor(ant_stick, 0, &cdesc);
    if (r < 0)
        throw LibusbException("libusb_get_config_descriptor", 0);

    try {
        if (cdesc->bNumInterfaces != 1)
        {
            throw std::runtime_error("unexpected number of interfaces");
        }
        const libusb_interface *i = cdesc->interface;
        if (i->num_altsetting != 1)
        {
            throw std::runtime_error("unexpected number of alternate settings");
        }
        const libusb_interface_descriptor *idesc = i->altsetting;

        for (int e = 0; e < idesc->bNumEndpoints; e++)
        {
            const libusb_endpoint_descriptor *edesc = idesc->endpoint + e;

            unsigned char edir = edesc->bEndpointAddress & LIBUSB_ENDPOINT_DIR_MASK;

            // NOTE: we technically look for the last read and write endpoints,
            // but there should be only one of each anyway.
            switch (edir) {
            case LIBUSB_ENDPOINT_IN:
                *read_endpoint = edesc->bEndpointAddress;
                break;
            case LIBUSB_ENDPOINT_OUT:
                *write_endpoint = edesc->bEndpointAddress;
                break;
            }
        }

        libusb_free_config_descriptor(cdesc);
    }
    catch (...)
    {
        libusb_free_config_descriptor(cdesc);
        throw;
    }
}

AntStick::AntStick()
    : m_Device (nullptr),
      m_DeviceHandle (nullptr),
      m_SerialNumber (0),
      m_Version (""),
      m_MaxNetworks (-1),
      m_MaxChannels (-1),
      m_Network(-1)
{
    try {
        m_Device = FindAntStick();
        if (! m_Device)
        {
            throw AntStickNotFound();
        }
        int r = libusb_open(m_Device, &m_DeviceHandle);
        if (r < 0)
        {
            m_DeviceHandle = nullptr;
            throw LibusbException("libusb_open", r);
        }
        ConfigureAntStick(m_DeviceHandle);

        unsigned char read_endpoint, write_endpoint;

        GetAntStickReadWriteEndpoints(
            m_Device, &read_endpoint, &write_endpoint);

        auto rt = std::unique_ptr<AntMessageReader>(
            new AntMessageReader (m_DeviceHandle, read_endpoint));
        m_Reader = std::move (rt);

        auto wt = std::unique_ptr<AntMessageWriter>(
            new AntMessageWriter (m_DeviceHandle, write_endpoint));
        m_Writer = std::move (wt);

        Reset();
        QueryInfo();

        m_LastReadMessage.reserve (1024);
    }
    catch (...)
    {
        // Need to clean up, as no one will do it for us....
        m_Reader = std::move (std::unique_ptr<AntMessageReader>());
        m_Writer = std::move (std::unique_ptr<AntMessageWriter>());
        if (m_DeviceHandle)
            libusb_close(m_DeviceHandle);
        libusb_unref_device(m_Device);
        throw;
    }
}

AntStick::~AntStick()
{
    m_Reader = std::move (std::unique_ptr<AntMessageReader>());
    m_Writer = std::move (std::unique_ptr<AntMessageWriter>());
    libusb_close(m_DeviceHandle);
    libusb_unref_device(m_Device);
}

void AntStick::WriteMessage(const Buffer &b)
{
    m_Writer->WriteMessage (b);
}

const Buffer& AntStick::ReadMessage()
{
    for(;;) 
    {
        m_Reader->GetNextMessage(m_LastReadMessage);
        if (SetAsideMessage(m_LastReadMessage))
            m_DelayedMessages.push(m_LastReadMessage);
        else
            return m_LastReadMessage;
    }
}

void AntStick::Reset()
{
    WriteMessage (MakeMessage (RESET_SYSTEM, 0));
    int ntries = 50;
    while (ntries-- > 0) {
        Buffer message = ReadMessage();
        if(message[2] == STARTUP_MESSAGE)
            break;
    }
}

void AntStick::QueryInfo()
{
    WriteMessage (MakeMessage (REQUEST_MESSAGE, 0, RESPONSE_SERIAL_NUMBER));
    Buffer msg_serial = ReadMessage();
    if (msg_serial[2] != RESPONSE_SERIAL_NUMBER)
        throw std::runtime_error ("QueryInfo: unexpected message");
    m_SerialNumber = msg_serial[3] | (msg_serial[4] << 8) | (msg_serial[5] << 16) | (msg_serial[6] << 24);

    WriteMessage (MakeMessage (REQUEST_MESSAGE, 0, RESPONSE_VERSION));
    Buffer msg_version = ReadMessage();
    if (msg_version[2] != RESPONSE_VERSION)
        throw std::runtime_error ("QueryInfo: unexpected message");
    const char *version = reinterpret_cast<const char *>(&msg_version[3]);
    m_Version = version;

    WriteMessage (MakeMessage (REQUEST_MESSAGE, 0, RESPONSE_CAPABILITIES));
    Buffer msg_caps = ReadMessage();
    if (msg_caps[2] != RESPONSE_CAPABILITIES)
        throw std::runtime_error ("QueryInfo: unexpected message");

    m_MaxChannels = msg_caps[3];
    m_MaxNetworks = msg_caps[4];
}

void AntStick::RegisterChannel (AntChannel *c)
{
    m_Channels.push_back (c);
}

void AntStick::UnregisterChannel (AntChannel *c)
{
    auto i = std::find (m_Channels.begin(), m_Channels.end(), c);
    m_Channels.erase(i);
}

void AntStick::SetNetworkKey (unsigned char key[8])
{
    unsigned char network = 0;          // always open network 0 for now

    m_Network = -1;
    Buffer nkey;
    nkey.push_back (network);
    nkey.insert (nkey.end(), &key[0], &key[8]);
    WriteMessage (MakeMessage (SET_NETWORK_KEY, nkey));
    Buffer response = ReadMessage();
    CheckChannelResponse (response, network, SET_NETWORK_KEY, 0);
    m_Network = network;
}

bool AntStick::MaybeProcessMessage(const Buffer &message)
{
    auto channel = message[3];

    if (message[2] == BURST_TRANSFER_DATA)
        channel = message[3] & 0x1f;

    for(auto i = m_Channels.begin(); i != m_Channels.end(); ++i) {
        if ((*i)->GetChannel() == channel)
        {
            (*i)->HandleMessage (&message[0], message.size());
            return true;
        }
    }

    return false;
}

void AntStick::Tick()
{
    if (m_DelayedMessages.empty())
    {
        m_Reader->MaybeGetNextMessage(m_LastReadMessage);
    }
    else
    {
        m_LastReadMessage = m_DelayedMessages.front();
        m_DelayedMessages.pop();
    }

    if (m_LastReadMessage.empty()) return;

    if (! MaybeProcessMessage (m_LastReadMessage))
    {
        std::cerr << "Unprocessed message:\n";
        DumpData (&m_LastReadMessage[0], m_LastReadMessage.size(), std::cerr);
    }
}


};                                      // end namespace AntfsTool
