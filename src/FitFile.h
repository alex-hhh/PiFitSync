
#pragma once
#include <stdint.h>
#include <iosfwd>
#include <stdexcept>
#include <vector>

namespace fit {

/** Buffer holding FIT data, as expected by ReadFitMessages. */
typedef std::vector<unsigned char> Buffer;

class FitDataBuffer;                    // Internal class

/** Base type for all FIT data types.  Contains common polymorphic
 * interface. */
struct FitTypeBase
{
    virtual ~FitTypeBase();
    virtual int TypeID() const = 0;
    virtual void ReadFrom(FitDataBuffer &buf) = 0;
};

/** Template class for defining FIT data types.  ID represents the type id, as
 * defined in the fit, BT is the base type name, such as an int32_t, NA is the
 * "not available" value for the type.  See below for the typedefs of all
 * available FIT types.
 */
template <int ID, typename BT, uint64_t NA>
struct FitType : public FitTypeBase
{
    template<int OID, typename OBT, uint64_t ONA>
    FitType(const FitType<OID, OBT, ONA>& other) {
        if (other.isNA())
            value = static_cast<BT>(NA);
        else
            value = static_cast<BT>(other.value);
    }

    FitType(const FitType &other) : value(other.value) { }
    FitType(BT v) : value (v) { }
    FitType() : value (static_cast<BT>(NA)) { }
    FitType(FitDataBuffer &buf) { ReadFrom(buf); }
    ~FitType();

    operator BT() const { return value; }

    FitType& operator= (const FitType &other) {
        value = other.value;
        return *this;
    }

    template<int OID, typename OBT, uint64_t ONA>
    FitType& operator=(const FitType<OID, OBT, ONA>& other) {
        if (other.isNA())
            value = static_cast<BT>(NA);
        else
            value = other.value;
        return *this;
    }

    // Read this value from a buffer.  This is implemented in fitfile.cpp,
    // meaning that new types cannot be added from outside this library.
    void ReadFrom(FitDataBuffer &buf) override;

    int TypeID() const override { return ID; }
    bool isNA() const { return value == static_cast<BT>(NA); }

    BT value;
};

/** The actual FIT data types, as defined by the FIT specification. 
 */
typedef FitType<0x00, uint8_t, 0xFF> FitEnum;
typedef FitType<0x01, int8_t, 0x7F> FitSint8;
typedef FitType<0x02, uint8_t, 0xFF> FitUint8;
typedef FitType<0x83, int16_t, 0x7FFF> FitSint16;
typedef FitType<0x84, uint16_t, 0xFFFF> FitUint16;
typedef FitType<0x85, int32_t, 0x7FFFFFFF> FitSint32;
typedef FitType<0x86, uint32_t, 0xFFFFFFFF> FitUint32;
typedef FitType<0x07, char, 0x00> FitChar; // base type for fit string type
typedef FitType<0x88, float, 0xFFFFFFFF> FitFloat32;
typedef FitType<0x89, double, 0xFFFFFFFFFFFFFFFF> FitFloat64;
typedef FitType<0x0A, uint8_t, 0x0> FitUint8z;
typedef FitType<0x8B, uint16_t, 0x0> FitUint16z;
typedef FitType<0x8C, uint32_t, 0x0> FitUint32z;
typedef FitType<0x0D, uint8_t, 0xFF> FitByte;

struct FitFileId
{
    FitEnum Type;
    FitEnum Manufacturer;
    FitUint16 Product;
    FitUint32z SerialNumber;
    FitUint32 TimeCreated;
};

std::ostream& operator<<(std::ostream &o, const FitFileId &m);

struct FitFileCreator
{
    FitUint16 SoftwareVersion;
    FitUint8 HardwareVersion;
};

std::ostream& operator<<(std::ostream &o, const FitFileCreator &m);

struct FitTimestampCorrelation
{
    FitUint32 Timestamp;
    FitUint16 FractionalTimestamp;
    FitUint32 SystemTimestamp;
    FitUint16 FractionalSystemTimestamp;
    FitUint32 LocalTimestamp;
    FitUint16 TimestampMs;
    FitUint16 SystemTimestampMs;
};

struct FitActivity
{
    FitUint32 Timestamp;
    FitUint32 TotalTimerTime;
    FitUint16 NumSessions;
    FitEnum Event;
    FitEnum EventType;
    FitUint32 LocalDateTime;
    FitUint8 EventGroup;
};

struct FitSession {
};

struct FitLap {
};

struct FitLength {
};

struct FitRecord {

};

struct FitEvent {

};

struct FitDeviceInfo {

};

struct FitTrainingFile {

};

struct FitHrv {

};

/** Builder class for FIT files.  A derived class needs to be implemented by
 * the client and passed to 'ReadFitMessages'.  The instance of the class will
 * receive "On..." notifications as messages are read from the data file.
 * Note that the methods are not virtual, so the client only needs to
 * implement handlers for messages they are interested in.
 *
 * @note ReadFitMessages is exception safe.  this means that an implementation
 * of this class can abort parsing by throwing an exception (not derived from
 * std::exception).  For example, code that only looks for file ID messages
 * can throw an exception in the OnFitFileId() method (which would need to be
 * caught).  This will cause the parsing to be aborted, which would save time
 * since this message is usually at the start of the file.
 */
class FitBuilder
{
public:
    virtual ~FitBuilder();
    virtual void OnFitFileId(const FitFileId &message);
    virtual void OnFitFileCreator (const FitFileCreator &message);
};

/** Read messages from the 'data' buffer and pass them to the FitBuilder
 * instance. */
void ReadFitMessages(Buffer &data, FitBuilder *b);

};                                      // end namespace fit
