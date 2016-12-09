#include "FitFile.h"

#include <iostream>
#include <vector>
#include <map>
#include <sstream>
#include <memory>

namespace {

const uint32_t fit_epoch = 631065600;


// ......................................................... BadFitFile ....

enum FitError {
    E_OK = 0,
    E_PARAM,
    E_HLEN,
    E_NOHDR,
    E_HCRC,
    E_SIG,
    E_NODATA,
    E_CRC
};

const char* FitErrorStr (int e)
{
    switch(e) {
    case E_OK: return "OK";
    case E_PARAM: return "bad parameters";
    case E_HLEN: return "bad header length";
    case E_NOHDR: return "short header";
    case E_HCRC: return "bad header checksum";
    case E_SIG: return "bad signature";
    case E_NODATA: return "short payload";
    case E_CRC: return "bad payload checksum";
    default: return "unknown";
    }
}

class BadFitFile : public std::exception
{
public:
    BadFitFile (const char *who, int error);
    virtual ~BadFitFile();

    const char* what() const noexcept(true);
    int ErrorCode() const { return m_Error; }
    
private:
    int m_Error;
    std::string m_Who;
    mutable std::string m_Message;
    mutable bool m_MessageDone;
};

BadFitFile::BadFitFile (const char *who, int error)
    : m_Error (error),
      m_Who (who),
      m_MessageDone (false)
{
    // empty
}

BadFitFile::~BadFitFile()
{
    // empty
}

const char* BadFitFile::what() const noexcept(true)
{
    if (! m_MessageDone) {
        std::ostringstream msg;
        msg << m_Who << ": " << FitErrorStr (m_Error);
        m_Message = msg.str();
        m_MessageDone = true;
    }
    return m_Message.c_str();
}



// .......................................................... BadTypeId ....

/** Exception thrown when an unknown FIT type Id is encountered. */
class BadTypeId : public std::exception
{
public:
    BadTypeId (const char *who, int type_id);
    virtual ~BadTypeId();

    const char* what() const noexcept(true);
    int TypeId() const { return m_TypeId; }

private:
    int m_TypeId;
    std::string m_Who;
    mutable std::string m_Message;
    mutable bool m_MessageDone;
};

BadTypeId::BadTypeId (const char *who, int type_id)
    : m_TypeId (type_id),
      m_Who (who),
      m_MessageDone (false)
{
    // empty
}

BadTypeId::~BadTypeId()
{
    // empty
}

const char* BadTypeId::what() const noexcept(true)
{
    if (! m_MessageDone) {
        std::ostringstream msg;
        msg << m_Who << ": bad FIT type id, " << m_TypeId;
        m_Message = msg.str();
        m_MessageDone = true;
    }
    return m_Message.c_str();
}


// ..................................................... BufferOverflow ....

/** Exception thrown when trying to read past the end of the data buffer. */
class BufferOverflow : public std::exception
{
public:
    BufferOverflow (const char *who);
    virtual ~BufferOverflow();

    const char* what() const noexcept(true);

private:
    std::string m_Who;
    mutable std::string m_Message;
    mutable bool m_MessageDone;
};

BufferOverflow::BufferOverflow (const char *who)
    : m_Who (who),
      m_MessageDone (false)
{
    // empty
}

BufferOverflow::~BufferOverflow()
{
    // empty
}

const char* BufferOverflow::what() const noexcept(true)
{
    if(! m_MessageDone) {
        std::ostringstream msg;
        msg << m_Who << ": buffer overflow";
        m_Message = msg.str();
        m_MessageDone = true;
    }
    return m_Message.c_str();
}


// .................................................. BadLocalMessageId ....

/** Exception thrown when the code encounters a local message ID that was not
 * defined yet.
 */
class BadLocalMessageId : public std::exception
{
public:
    BadLocalMessageId (const char *who, int local_id);
    ~BadLocalMessageId();

    const char* what() const noexcept(true);
    int LocalMessageId() const { return m_LocalMessageId; }
    
private:
    int m_LocalMessageId;
    std::string m_Who;
    mutable std::string m_Message;
    mutable bool m_MessageDone;    
};

BadLocalMessageId::BadLocalMessageId (const char *who, int local_id)
    : m_LocalMessageId (local_id),
      m_Who (who),
      m_MessageDone (false)
{
    // empty
}
      
BadLocalMessageId::~BadLocalMessageId()
{
    // empty
}

const char* BadLocalMessageId::what() const noexcept(true)
{
    if (! m_MessageDone) {
        std::ostringstream msg;
        msg << m_Who << ": unknown local message id, " << m_LocalMessageId;
        m_Message = msg.str();
        m_MessageDone = true;
    }
    return m_Message.c_str();
}

};                                      // end anonymous namespace

namespace {
using namespace fit;

bool IsMachineBigEndian()
{
    union { unsigned u; char c; } test_value;
    test_value.u = 1;
    return test_value.c == 0;
}

// ................................................. Additional Helpers ....

int TypeSize(int type)
{
    switch (type) {
    case 0x00: return 1;
    case 0x01: return 1;
    case 0x02: return 1;
    case 0x83: return 2;
    case 0x84: return 2;
    case 0x85: return 4;
    case 0x86: return 4;
    case 0x07: return 1;
    case 0x88: return 4;
    case 0x89: return 8;
    case 0x0A: return 1;
    case 0x8B: return 2;
    case 0x8C: return 4;
    case 0x0D: return 1;
    default:
        throw BadTypeId ("TypeSize()", type);
    }
}

template <typename T> T CastAs (const FitTypeBase &other)
{
    switch (other.TypeID()) {
    case 0x00: return T(static_cast<const FitEnum&>(other));
    case 0x01: return T(static_cast<const FitSint8&>(other));
    case 0x02: return T(static_cast<const FitUint8&>(other));
    case 0x83: return T(static_cast<const FitSint16&>(other));
    case 0x84: return T(static_cast<const FitUint16&>(other));
    case 0x85: return T(static_cast<const FitSint32&>(other));
    case 0x86: return T(static_cast<const FitUint32&>(other));
    case 0x07: return T(static_cast<const FitChar&>(other));
    case 0x88: return T(static_cast<const FitFloat32&>(other));
    case 0x89: return T(static_cast<const FitFloat64&>(other));
    case 0x0A: return T(static_cast<const FitUint8z&>(other));
    case 0x8B: return T(static_cast<const FitUint16z&>(other));
    case 0x8C: return T(static_cast<const FitUint32z&>(other));
    case 0x0D: return T(static_cast<const FitByte&>(other));
    default:
        throw BadTypeId ("CastAs()", other.TypeID());
    }
}

std::unique_ptr<FitTypeBase> ReadValue(int type_id, FitDataBuffer &buf)
{
    switch (type_id) {
    case 0x00: return std::make_unique<FitEnum>(buf);
    case 0x01: return std::make_unique<FitSint8>(buf);
    case 0x02: return std::make_unique<FitUint8>(buf);
    case 0x83: return std::make_unique<FitSint16>(buf);
    case 0x84: return std::make_unique<FitUint16>(buf);
    case 0x85: return std::make_unique<FitSint32>(buf);
    case 0x86: return std::make_unique<FitUint32>(buf);
    case 0x07: return std::make_unique<FitChar>(buf);
    case 0x88: return std::make_unique<FitFloat32>(buf);
    case 0x89: return std::make_unique<FitFloat64>(buf);
    case 0x0A: return std::make_unique<FitUint8z>(buf);
    case 0x8B: return std::make_unique<FitUint16z>(buf);
    case 0x8C: return std::make_unique<FitUint32z>(buf);
    case 0x0D: return std::make_unique<FitByte>(buf);
    default:
        throw BadTypeId ("ReadValue()", type_id);
    }
}


}; // end anonymous namespace

namespace fit {


// ...................................................... FitDataBuffer ....

class FitDataBuffer {
public:
    FitDataBuffer();
    void SetBuffer (int protocol, int profile, unsigned char *data, int len);

    void SetBigEndian (bool isBig) {
        if (m_MachineIsBigEndian)
            m_RevertBytes = ! isBig;
        else
            m_RevertBytes = isBig;
    }
    bool ShouldRevertBytes() const { return m_RevertBytes; }
    int ProtocolVersion() const { return m_ProtocolVersion; }
    int ProfileVersion() const { return m_ProfileVersion; }
    int Length() const { return m_Limit; }
    void SkipBytes(int num) { m_Pos += num; }

    unsigned char ReadByte()
        {
            if (DataValid() && ! IsEof())
                return m_Data[m_Pos++];
            else
                throw BufferOverflow ("FitDataBuffer::ReadByte()");
        }
    
    bool DataValid () const { return m_Data != nullptr; }
    bool IsEof() const { return m_Pos >= m_Limit; }

private:
    int m_ProtocolVersion;
    int m_ProfileVersion;
    unsigned char *m_Data;
    int m_Pos;
    int m_Limit;
    bool m_RevertBytes;
    bool m_MachineIsBigEndian;
};


FitDataBuffer::FitDataBuffer()
    : m_ProtocolVersion (0),
      m_ProfileVersion (0),
      m_Data (nullptr),
      m_Pos (0),
      m_Limit (0),
      m_RevertBytes (false),
      m_MachineIsBigEndian (IsMachineBigEndian())
{
    // empty
}

void FitDataBuffer::SetBuffer (int protocol, int profile, unsigned char *data, int len)
{
    m_ProtocolVersion = protocol;
    m_ProfileVersion = profile;
    m_Data = data;
    m_Pos = 0;
    m_Limit = len;
    m_RevertBytes = false;
}


// ........................................................... FitTypes ....

FitTypeBase::~FitTypeBase()
{
    // empty
}

template <int ID, typename BT, uint64_t NA>
FitType<ID, BT, NA>::~FitType()
{
    // empty
}

template <int ID, typename BT, uint64_t NA>
void FitType<ID, BT, NA>::ReadFrom(FitDataBuffer &buf)
{
    union {
        BT b;
        uint8_t d[sizeof(BT)];
    } val;
    
    if (buf.ShouldRevertBytes()) {
        for (unsigned i = sizeof(BT) - 1; i >= 0; --i)
            val.d[i] = buf.ReadByte();
    } else {
        for (unsigned i = 0; i < sizeof(BT); ++i)
            val.d[i] = buf.ReadByte();
    }
    value = val.b;
}

};                                      // end namespace fit

namespace {
using namespace fit;

enum GlobalMessageNumber
{
    GMN_FILE_ID = 0,
    GMN_FILE_CREATOR = 49,
    GMN_TIMESTAMP_CORRELATION = 162
};


// ..................................................... MessageBuilder ....

class MessageBuilder
{
public:
    typedef std::unique_ptr<FitTypeBase> TypePtr;
    typedef std::vector<std::unique_ptr<FitTypeBase>> ArrayPtr;

    virtual ~MessageBuilder() {}
    virtual void MessageBegin() {}
    virtual void ProcessValue (int /*fieldNum*/, TypePtr /*value*/) {}
    virtual void ProcessArrayValue(int /*fieldNum*/, const ArrayPtr &/*value*/) {}
    virtual void MessageDone() {}
};


// ................................................... FitFileIdBuilder ....

class FitFileIdBuilder : public MessageBuilder
{
public:
    FitFileIdBuilder(FitBuilder *b);
    ~FitFileIdBuilder();
    void ProcessValue (int fieldNum, TypePtr value) override;
    void MessageDone() override;

private:
    FitFileId m_Message;
    FitBuilder *m_Builder;
};

FitFileIdBuilder::FitFileIdBuilder(FitBuilder *b)
    : m_Builder(b)
{
    // empty
}

FitFileIdBuilder::~FitFileIdBuilder()
{
    // empty
}

void FitFileIdBuilder::ProcessValue (int fieldNum, MessageBuilder::TypePtr value)
{
    const FitTypeBase &v = *value.get();
    switch (fieldNum) {
    case 0: m_Message.Type = CastAs<FitEnum>(v); break;
    case 1: m_Message.Manufacturer = CastAs<FitEnum>(v); break;
    case 2: m_Message.Product = CastAs<FitUint16>(v); break;
    case 3: m_Message.SerialNumber = CastAs<FitUint32z>(v); break;
    case 4: m_Message.TimeCreated = fit_epoch + CastAs<FitUint32>(v); break;
        // silently ignore all other field types
    }
}

void FitFileIdBuilder::MessageDone()
{
    if (m_Builder)
        m_Builder->OnFitFileId(m_Message);
}


// .............................................. FitFileCreatorBuilder ....

class FitFileCreatorBuilder : public MessageBuilder
{
public:
    FitFileCreatorBuilder(FitBuilder *b);
    ~FitFileCreatorBuilder();
    void ProcessValue (int fieldNum, TypePtr value) override;
    void MessageDone() override;

private:
    FitFileCreator m_Message;
    FitBuilder *m_Builder;
};

FitFileCreatorBuilder::FitFileCreatorBuilder(FitBuilder *b)
    : m_Builder(b)
{
    // empty
}

FitFileCreatorBuilder::~FitFileCreatorBuilder()
{
    // empty
}

void FitFileCreatorBuilder::ProcessValue (int fieldNum, MessageBuilder::TypePtr value)
{
    const FitTypeBase &v = *value.get();
    switch (fieldNum) {
    case 0: m_Message.SoftwareVersion = CastAs<FitUint16>(v); break;
    case 1: m_Message.HardwareVersion = CastAs<FitUint8>(v); break;
        // silently ignore all other field types
    }
}

void FitFileCreatorBuilder::MessageDone()
{
    if (m_Builder)
        m_Builder->OnFitFileCreator(m_Message);
}



// .............................................. MessageBuilderFactory ....

class MessageBuilderFactory {
public:
    typedef std::unique_ptr<MessageBuilder> BuilderPtr;

    MessageBuilderFactory(FitBuilder *b);
    ~MessageBuilderFactory();
    BuilderPtr MakeBuilder(unsigned global_message);

private:
    FitBuilder *m_Builder;
};

MessageBuilderFactory::MessageBuilderFactory(FitBuilder *b)
    : m_Builder(b)
{
    // empty
}

MessageBuilderFactory::~MessageBuilderFactory()
{
    // empty
}

MessageBuilderFactory::BuilderPtr MessageBuilderFactory::MakeBuilder(unsigned global_message)
{
    BuilderPtr builder;
    switch (global_message) {
    case GMN_FILE_ID: builder = std::make_unique<FitFileIdBuilder>(m_Builder); break;
    case GMN_FILE_CREATOR: builder = std::make_unique<FitFileCreatorBuilder>(m_Builder); break;
    }
    return builder;
}


// .......................................................... FitReader ....

struct FieldDef
{
    FieldDef (uint8_t num, uint8_t  sz, uint8_t type);
    uint8_t Number;
    uint8_t Size;
    uint8_t BaseType;
    uint8_t ValueCount;
};

FieldDef::FieldDef (uint8_t num, uint8_t  sz, uint8_t type)
    : Number (num),
      Size (sz),
      BaseType (type)
{
    int tsz = TypeSize(BaseType);
    if (sz % tsz) {
        // sz needs to be a multiple of the base type size.
        std::ostringstream msg;
        msg << "FieldDef: invalid size " << (int)sz << ", expecting multiple of " << tsz;
        throw std::runtime_error(msg.str());
    }
    ValueCount = sz / tsz;
}

struct DevFieldDef
{
    DevFieldDef (uint8_t num, uint8_t sz, uint8_t dev)
        : Number (num), Size (sz), DevIndex (dev) {/* empty */}
    uint8_t Number;
    uint8_t Size;
    uint8_t DevIndex;
};

struct MessageDef
{
    int LocalNumber;
    int GlobalNumber;
    bool BigEndian;
    /** Size of the data message for this message definition.  This can be
     * computed by adding the sizes of a Fields and DevFields in this
     * structure, but it is cached here.
     */
    int DataMessageSize;
    int DevFieldsSize;
    std::vector<FieldDef> Fields;
    std::vector<DevFieldDef> DevFields;
};

std::ostream& operator<<(std::ostream &o, const MessageDef &m)
{
    o << "#<MDEF local: " << m.LocalNumber << " global: " << m.GlobalNumber
      << " size: " << m.DataMessageSize << " fields: " << m.Fields.size()
      << " dev fields: " << m.DevFields.size() << ">";
    return o;
}

class FitReader {
public:
    FitReader(FitDataBuffer *db, FitBuilder *b);
    void ReadMessages();

private:
    void ReadMessageDef (int header);
    void BuildMessage(const MessageDef &mdef, uint32_t timestamp);

    uint32_t m_Timestamp;
    FitDataBuffer *m_DataBuffer;
    MessageBuilderFactory m_Factory;
    std::map<int, MessageDef> m_Definitions;
};

FitReader::FitReader(FitDataBuffer *db, FitBuilder *b)
    : m_DataBuffer(db),	m_Factory(b)
{
    // empty
}

void FitReader::ReadMessageDef (int header)
{
    MessageDef mdef;
    mdef.LocalNumber = header & 0x0F;
    m_DataBuffer->ReadByte();                     // skip reserved byte
    mdef.BigEndian = (m_DataBuffer->ReadByte() != 0);
    m_DataBuffer->SetBigEndian (mdef.BigEndian);
    mdef.GlobalNumber = FitUint16 (*m_DataBuffer);
    int nfields = m_DataBuffer->ReadByte();
    for (int i = 0; i < nfields; ++i) {
        uint8_t num = m_DataBuffer->ReadByte();
        uint8_t sz = m_DataBuffer->ReadByte();
        uint8_t type = m_DataBuffer->ReadByte();
        mdef.Fields.push_back (FieldDef (num, sz, type));
    }
    if (header & 0x20) {              // message has developer specific fields
        nfields = m_DataBuffer->ReadByte();
        for (int i = 0; i < nfields; ++i) {
            uint8_t num = m_DataBuffer->ReadByte();
            uint8_t sz = m_DataBuffer->ReadByte();
            uint8_t dev = m_DataBuffer->ReadByte();
            mdef.DevFields.push_back (DevFieldDef (num, sz, dev));
        }
    }
    int fsize = 0;
    for (auto i = std::begin (mdef.Fields); i != std::end (mdef.Fields); ++i)
        fsize += i->Size;
    int dsize = 0;
    for (auto i = std::begin (mdef.DevFields); i != std::end (mdef.DevFields); ++i)
        dsize += i->Size;
    mdef.DataMessageSize = fsize + dsize;
    mdef.DevFieldsSize = dsize;
    // std::cout << mdef << "\n";
    m_Definitions[mdef.LocalNumber] = mdef;
}

void FitReader::ReadMessages ()
{
    while (! m_DataBuffer->IsEof()) {
        unsigned char header = m_DataBuffer->ReadByte();
        if (header & 0x40) {
            ReadMessageDef (header);
        } else if (header & 0x80) {
            // compressed time stamp
            int local = header & 0x60;
            int offset = header & 0x1F;
            auto pos = m_Definitions.find(local);
            if (pos == m_Definitions.end()) {
                throw BadLocalMessageId ("FitReader::ReadMessages", local);
            } else {
                BuildMessage(pos->second, m_Timestamp + offset);
            }
        } else {
            // plain data message
            int local = header & 0x0F;
            auto pos = m_Definitions.find(local);
            if (pos == m_Definitions.end()) {
                throw BadLocalMessageId ("FitReader::ReadMessages", local);
            } else {
                BuildMessage(pos->second, m_Timestamp);
            }
        }
    }
}

void FitReader::BuildMessage(const MessageDef &mdef, uint32_t timestamp)
{
    auto builder = m_Factory.MakeBuilder(mdef.GlobalNumber);
    if (! builder) {
        for (auto i = begin (mdef.Fields); i != end (mdef.Fields); ++i) {
            if (i->Number == 253) {
                auto v = ReadValue (i->BaseType, *m_DataBuffer);
                m_Timestamp = CastAs<FitUint32>(*v);
            } else {
                m_DataBuffer->SkipBytes(i->Size);
            }
        }
        m_DataBuffer->SkipBytes(mdef.DevFieldsSize);
        return;
    }

    bool timestamp_seen = false;
    builder->MessageBegin();
    for (auto i = begin (mdef.Fields); i != end (mdef.Fields); ++i) {
        if (i->ValueCount > 1) {        // an array
            std::vector<std::unique_ptr<FitTypeBase>> v;
            for (int j = 0; j < i->ValueCount; ++j) {
                auto w = ReadValue (i->BaseType, *m_DataBuffer);
                v.push_back (std::move(w));
            }
            builder->ProcessArrayValue (i->Number, v);
        } else {
            auto v = ReadValue (i->BaseType, *m_DataBuffer);
            builder->ProcessValue (i->Number, std::move(v));
            if (i->Number == 253) {
                timestamp_seen = true;
                m_Timestamp = CastAs<FitUint32>(*v);
            }
        }
    }
    if (! timestamp_seen) {
	// pass in the received timestamp value, not m_Timestamp,
	// as the received one has an offset applied to it.
        builder->ProcessValue(253, std::make_unique<FitUint32>(timestamp));
    }
    // TODO: need to read def fields
    m_DataBuffer->SkipBytes (mdef.DevFieldsSize);
    builder->MessageDone();
}

uint16_t Crc16(const unsigned char *data, int len)
{
    static const uint16_t crc_table[16] = {
        0x0000, 0xCC01, 0xD801, 0x1400, 0xF001, 0x3C00, 0x2800, 0xE401,
        0xA001, 0x6C00, 0x7800, 0xB401, 0x5000, 0x9C01, 0x8801, 0x4400
    };

    uint16_t crc = 0;
    while (len--) {
        unsigned char byte = *data++;
        // compute checksum of lower four bits of byte
        uint16_t tmp = crc_table[crc & 0xF];
        crc = (crc >> 4) & 0x0FFF;
        crc = crc ^ tmp ^ crc_table[byte & 0xF];
        // now compute checksum of upper four bits of byte
        tmp = crc_table[crc & 0xF];
        crc = (crc >> 4) & 0x0FFF;
        crc = crc ^ tmp ^ crc_table[(byte >> 4) & 0xF];
    }
    return crc;
}

int GetChunk (unsigned char *data, uint32_t length, fit::FitDataBuffer *buf, unsigned char **rest)
{
    if (data == nullptr || length < 1)
        return E_PARAM;
    uint32_t hlen = data[0];            // first byte is the header length
    if (hlen != 12 && hlen != 14)       // which must be 12 or 14 (with CRC)
        return E_HLEN;
    if (length < hlen)
        return E_NOHDR;
    if (hlen == 14 && (data[12] || data[13])) {
        // Header has a non-zero CRC, check it
        if (Crc16 (data, hlen) != 0) {
            return E_HCRC;
        }
    }
    if (data[8] != '.' || data[9] != 'F' || data[10] != 'I' || data[11] != 'T') {
        return E_SIG;
    }
    uint32_t payload = (data[4] | (data[5] << 8) | (data[6] << 16) | (data[7] << 24));
    if (length < (hlen + payload + 2)) {
        return E_NODATA;
    }
    if (data[hlen + payload] || data[hlen + payload + 1]) {
        // last two bytes have a non-zero CRC, check it
        if (Crc16 (data, hlen + payload + 2) != 0) {
            return E_CRC;
        }
    }
    // Header looks OK, decode it and fill in c and rest
    if (buf) {
        buf->SetBuffer (
            data[1],
            data[2] | (data[3] << 8),
            data + hlen,
            payload);
    }

    if (rest) {
        if (length == hlen + payload + 2) {
            *rest = nullptr;            // no more chunks in the FIT file
        } else {
            *rest = data + hlen + payload + 2;
        }
    }

    return E_OK;
}


};                                      // end anonymous


namespace fit {

std::ostream& operator<<(std::ostream &o, const FitFileId &m)
{
    o << "#<FileId ";
    o << "Type: ";
    if(m.Type.isNA()) {
        o << "NA";
    } else {
        o << (int)m.Type.value;
    }
    o << " Manufacturer: ";
    if (m.Manufacturer.isNA()) {
        o << "NA";
    } else {
        o << (int)m.Manufacturer.value;
    }
    o << " Product: ";
    if (m.Product.isNA()) {
        o << "NA";
    } else {
        o << m.Product.value;
    }
    o << " SerialNumber: ";
    if (m.SerialNumber.isNA()) {
        o << "NA";
    } else {
        o << m.SerialNumber.value;
    }
    o << " Created: ";
    if (m.TimeCreated.isNA()) {
        o << "NA";
    } else {
        o << m.TimeCreated.value;
    }
    o << " >";
    return o;
}

std::ostream& operator<<(std::ostream &o, const FitFileCreator &m)
{
    o << "#<FileCreator ";
    o << "SwVer: ";
    if (m.SoftwareVersion.isNA()) {
        o << "NA";
    } else {
        o << m.SoftwareVersion;
    }
    o  << " HwVer: ";
    if (m.HardwareVersion.isNA()) {
        o << "NA";
    } else {
        o << (int)m.HardwareVersion;
    }
    o << " >";
    return o;
}

FitBuilder::~FitBuilder()
{
    // empty
}

void FitBuilder::OnFitFileId(const FitFileId &)
{
    // empty
}

void FitBuilder::OnFitFileCreator (const FitFileCreator &)
{
    // empty
}

void ReadFitMessages(Buffer &data, FitBuilder *b)
{
    unsigned char *buf = &data[0];
    while (buf) {
        unsigned char *rest = nullptr;
        auto remain = data.size() - (buf - &data[0]);
        fit::FitDataBuffer chunk;
        auto r = GetChunk (buf, remain, &chunk, &rest);
        if (r != E_OK) {
            std::ostringstream msg;
            msg << "ReadFitMessages(@" << buf - &data[0] << ")";
            throw BadFitFile (msg.str().c_str(), r);
        }

//        std::cout << "Chunk @" << (void*)buf << ": Protocol " << chunk.ProtocolVersion()
//                  << ", Profile " << chunk.ProfileVersion()
//                  << ", Data Length " << chunk.Length() << "\n";
//
        FitReader reader(&chunk, b);
        reader.ReadMessages();
        buf = rest;
    }
}

}; // end namespace fit
