#pragma once

#include "Tools.h"

namespace FitSync {


// ................................................... AntMessageReader ....

/** Read ANT messages from an USB device (the ANT stick) */
class AntMessageReader
{
public:
    AntMessageReader (libusb_device_handle *dh, unsigned char endpoint);
    ~AntMessageReader();

    /** Fill `message' with the next available message.  If no message is
     * received within a small amount of time, an empty buffer is returned.
     * If a message is returned, it is a valid message (good header, length
     * and checksum). */
    void MaybeGetNextMessage(Buffer &message);

    /** Fill `message' with the next available message.  If a message is
     * returned, it is a valid message (good header, length and checksum).  If
     * no message is received within a small amount of time, a timeout
     * exception will be thrown. */
    void GetNextMessage (Buffer &message);

private:

    void GetNextMessage1(Buffer &message);

    static void LIBUSB_CALL Trampoline (libusb_transfer *);
    void SubmitUsbTransfer();
    void CompleteUsbTransfer(const libusb_transfer *);
    
    libusb_device_handle *m_DeviceHandle;
    unsigned char m_Endpoint;
    libusb_transfer *m_Transfer;

    /** Hold partial data received from the USB stick.  A single USB read
     * might not return an entire ANT message. */
    Buffer m_Buffer;
    unsigned m_Mark;            // buffer position up to where data is available
    bool m_Active;              // is there a transfer active?
};


// ................................................... AntMessageWriter ....

/** Write ANT messages from an USB device (the ANT stick). */
class AntMessageWriter
{
public:
    AntMessageWriter (libusb_device_handle *dh, unsigned char endpoint);
    ~AntMessageWriter();

    /** Write `message' to the USB device.  This is presumably an ANT message,
     * but we don't check.  When this function returns, the message has been
     * written (there is no buffering on the application side). An exception
     * is thrown if there is an error or a timeout. */
    void WriteMessage (const Buffer &message);

private:
    
    static void LIBUSB_CALL Trampoline (libusb_transfer *);
    void SubmitUsbTransfer();
    void CompleteUsbTransfer(const libusb_transfer *);

    libusb_device_handle *m_DeviceHandle;
    unsigned char m_Endpoint;
    libusb_transfer *m_Transfer;

    Buffer m_Buffer;
    bool m_Active;                      // is there a transfer active?
};


};

/*
  Local Variables:
  mode: c++
  End:
*/
