#pragma once

#include "AntMessage.h"
#include <memory>
#include <queue>

namespace FitSync
{

class AntMessageReader;
class AntMessageWriter;
class AntStick;


// ......................................................... AntChannel ....

class AntChannel 
{
public:
    AntChannel (AntStick *stick, int num, AntChannelType type, 
                unsigned period, unsigned char timeout, unsigned char frequency);
    virtual ~AntChannel();

    /** Return the channel id for this channel. */
    int GetChannel() { return m_ChannelNumber; }

    /** Process a message received on this channel.  This will look for some
     * channel events, and process them, but delegate most of the messages to
     * ProcessMessage() in the derived class.
     */
    void HandleMessage(const unsigned char *data, int size);

    /** Request this channel to close.  Closing the channel involves receiving
     * a status message back, so HandleMessage() stil has to be called with
     * chanel messages until IsOpen() returns false.
     */
    void RequestClose();

    /** Return true if this channel is closed. */
    bool IsOpen() { return m_IsOpen; };

protected:

    /** Process a message received on this channel. */
    virtual void ProcessMessage (const unsigned char *data, int size) = 0;

    void Configure (unsigned period, unsigned char timeout, unsigned char frequency);

    bool m_IsOpen;              // true if this channel is open.
    unsigned char m_ChannelNumber;
    AntStick *m_Stick;
};


// ........................................................... AntStick ....

/** Exception thrown when the ANT stick is not found (perhaps because it is
    not plugged into a USB port). */
class AntStickNotFound : public std::exception
{
public:
    const char * what() const noexcept override;
};

class AntStick
{
  friend AntChannel;

public:
  AntStick();
  ~AntStick();

  void SetNetworkKey (unsigned char key[8]);

  unsigned GetSerialNumber() const { return m_SerialNumber; }
  std::string GetVersion() const { return m_Version; }
  int GetMaxNetworks() const { return m_MaxNetworks; }
  int GetMaxChannels() const { return m_MaxChannels; }
  int GetNetwork() const { return m_Network; }

  void WriteMessage(const Buffer &b);
  const Buffer& ReadMessage();
    
  void Tick();

private:

  void Reset();
  void QueryInfo();
  void RegisterChannel (AntChannel *c);
  void UnregisterChannel (AntChannel *c);

  bool MaybeProcessMessage(const Buffer &message);

  libusb_device *m_Device;
  libusb_device_handle *m_DeviceHandle;

  unsigned m_SerialNumber;
  std::string m_Version;
  int m_MaxNetworks;
  int m_MaxChannels;

  int m_Network;

  std::queue <Buffer> m_DelayedMessages;
  Buffer m_LastReadMessage;

  std::unique_ptr<AntMessageReader> m_Reader;
  std::unique_ptr<AntMessageWriter> m_Writer;

  std::vector<AntChannel*> m_Channels;
};


};                                      // end namespace AntfsTool

/*
    Local Variables:
    mode: c++
    End:
*/
