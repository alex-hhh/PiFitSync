#pragma once

#include <libusb.h>

#include <vector>
#include <iosfwd>
#include <string>
#include <sstream>

#include <string.h>

namespace FitSync {

  typedef std::vector<unsigned char> Buffer;


// .................................................... LibusbException ....

/** Convenience class to throw exception with USB error codes and get proper
 * error names for them. */
class LibusbException : public std::exception
{
public:
  LibusbException(const char *who, int error_code)
    : m_Who(who), m_ErrorCode(error_code), m_MessageDone(false)
  {
  }

  const char* what() const noexcept(true)
  {
    if (!m_MessageDone) {
      std::ostringstream msg;
      msg << m_Who << ": (" << m_ErrorCode << ") "
          << libusb_error_name(m_ErrorCode);
      m_Message = msg.str();
      m_MessageDone = true;
    }
    return m_Message.c_str();
  }

  int error_code() { return m_ErrorCode; }

private:
  std::string m_Who;
  int m_ErrorCode;
  mutable std::string m_Message;
  mutable bool m_MessageDone;
};

/** Print a hex dump of 'data' to the stream 'o'.  The data is printed on
 * lines with the address, character representation and hex representation on
 * each line.  This hopefully makes it easy to determine the contents of both
 * character and binary data.
 */
void DumpData (const unsigned char *data, int size, std::ostream &o);

/** Put the current time on the output stream o. */
void PutTimestamp(std::ostream &o);


};                                      // end namespace FitSync

/*
    Local Variables:
    mode: c++
    End:
*/
