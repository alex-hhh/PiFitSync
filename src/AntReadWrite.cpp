
#include "AntReadWrite.h"
#include "AntMessage.h"
#include <assert.h>
#include <iterator>
#include <stdexcept>

namespace FitSync {


// ................................................... AntMessageReader ....

AntMessageReader::AntMessageReader (libusb_device_handle *dh, unsigned char endpoint)
    : m_DeviceHandle (dh),
      m_Endpoint (endpoint),
      m_Transfer (nullptr),
      m_Mark(0),
      m_Active (false)
{
    m_Buffer.reserve (1024);
    m_Transfer = libusb_alloc_transfer (0);
}

AntMessageReader::~AntMessageReader()
{
    if (m_Active) {
        libusb_cancel_transfer (m_Transfer);
    }
    while (m_Active) {
        libusb_handle_events (nullptr);
    }
    libusb_free_transfer (m_Transfer);
}



void AntMessageReader::MaybeGetNextMessage (Buffer &message)
{
    message.clear();

    if (m_Active) 
    {
        // Finish the transfer, wait 2 seconds for it
        struct timeval tv;
        tv.tv_sec = 2; tv.tv_usec = 0;
        int r = libusb_handle_events_timeout_completed (nullptr, &tv, nullptr);
        if (r < 0)
            throw LibusbException ("libusb_handle_events", r);
    }

    if (m_Active)
    {
        return;
    }

    GetNextMessage1(message);
}

void AntMessageReader::GetNextMessage(Buffer &message)
{
    MaybeGetNextMessage(message);
    if (message.empty())
        throw std::runtime_error ("AntMessageReader -- timed out");
}

void AntMessageReader::GetNextMessage1(Buffer &message)
{
    // Cannot operate on the buffer while a transfer is active
    assert (! m_Active);

    // Look for the sync byte which starts a message
    while ((! m_Buffer.empty()) && m_Buffer[0] != SYNC_BYTE)
    {
        m_Buffer.erase(m_Buffer.begin());
        m_Mark--;
    }

    // An ANT message has the following sequence: SYNC, LEN, MSGID, DATA,
    // CHECKSUM.  An empty message has at least 4 bytes in it.
    if (m_Mark < 4)
    {
        SubmitUsbTransfer();
        return MaybeGetNextMessage(message);
    }

    // LEN is the length of the data, actual message length is LEN + 4.
    unsigned len = m_Buffer[1] + 4;

    if (m_Mark < len)
    {
        SubmitUsbTransfer();
        return MaybeGetNextMessage(message);
    }

    std::copy(m_Buffer.begin(), m_Buffer.begin() + len, std::back_inserter(message));
    // Remove the message from the buffer.
    m_Buffer.erase (m_Buffer.begin(), m_Buffer.begin() + len);
    m_Mark -= len;

    if (! IsGoodChecksum (message))
    {
        throw std::runtime_error ("AntMessageReader -- bad checksum");
    }
}

void LIBUSB_CALL AntMessageReader::Trampoline (libusb_transfer *t)
{
    AntMessageReader *a = reinterpret_cast<AntMessageReader*>(t->user_data);
    a->CompleteUsbTransfer (t);
}

void AntMessageReader::SubmitUsbTransfer()
{
    assert (! m_Active);

    const int read_size = 128;
    const int timeout = 10000;

    // Make sure we have enough space in the buffer.
    m_Buffer.resize (m_Mark + read_size);

    libusb_fill_bulk_transfer (
        m_Transfer, m_DeviceHandle, m_Endpoint,
        &m_Buffer[m_Mark], read_size, Trampoline, this, timeout);

    int r = libusb_submit_transfer (m_Transfer);
    if (r < 0)
        throw LibusbException ("libusb_submit_transfer", r);

    m_Active = true;
}

void AntMessageReader::CompleteUsbTransfer(const libusb_transfer *t)
{
    assert (t == m_Transfer);

    m_Active = false;

    bool ok = (m_Transfer->status == LIBUSB_TRANSFER_COMPLETED);

    m_Mark += ok ? m_Transfer->actual_length : 0;
    m_Buffer.erase (m_Buffer.begin() + m_Mark, m_Buffer.end());
}


// ................................................... AntMessageWriter ....

AntMessageWriter::AntMessageWriter (libusb_device_handle *dh, unsigned char endpoint)
    : m_DeviceHandle (dh), m_Endpoint (endpoint), m_Transfer (nullptr), m_Active (false)
{
    m_Transfer = libusb_alloc_transfer (0);
}

AntMessageWriter::~AntMessageWriter()
{
    if (m_Active) {
        libusb_cancel_transfer (m_Transfer);
    }
    while (m_Active) {
        libusb_handle_events (nullptr);
    }
    libusb_free_transfer (m_Transfer);
}

void AntMessageWriter::WriteMessage (const Buffer &message)
{
    assert (! m_Active);
    m_Buffer = message;
    m_Active = false;
    SubmitUsbTransfer();

    // Finish the transfer, wait 2 seconds for it
    struct timeval tv;
    tv.tv_sec = 2; tv.tv_usec = 0;
    int r = libusb_handle_events_timeout_completed (nullptr, &tv, nullptr);
    if (r < 0)
        throw LibusbException ("libusb_handle_events", r);

    if (! m_Active && m_Transfer->status != LIBUSB_TRANSFER_COMPLETED)
        throw LibusbException ("AntMessageWriter", m_Transfer->status);

    if (m_Active)
    {
        libusb_cancel_transfer (m_Transfer);
        struct timeval tv;
        tv.tv_sec = 0; tv.tv_usec = 10 * 1000;
        int r = libusb_handle_events_timeout_completed (nullptr, &tv, nullptr);
        if (r < 0)
            throw LibusbException ("libusb_handle_events", r);

        m_Active = false;               // ready or not!

        throw std::runtime_error ("AntMessageWriter -- timed out");
    }
}

void LIBUSB_CALL AntMessageWriter::Trampoline (libusb_transfer *t)
{
    AntMessageWriter *a = reinterpret_cast<AntMessageWriter*>(t->user_data);
    a->CompleteUsbTransfer (t);
}

void AntMessageWriter::SubmitUsbTransfer()
{
    const int timeout = 2000;

    libusb_fill_bulk_transfer (
        m_Transfer, m_DeviceHandle, m_Endpoint,
        &m_Buffer[0], m_Buffer.size(), Trampoline, this, timeout);

    int r = libusb_submit_transfer (m_Transfer);
    if (r < 0)
        throw LibusbException ("libusb_submit_transfer", r);

    m_Active = true;
}

void AntMessageWriter::CompleteUsbTransfer(const libusb_transfer *t)
{
    assert (t == m_Transfer);
    m_Active = false;
}


};                                      // end namespace AntfsTool
