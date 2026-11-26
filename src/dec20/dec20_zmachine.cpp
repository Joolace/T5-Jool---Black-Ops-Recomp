// Z-machine interpreter (Standard 1.1) for version 3 story files.
#include "dec20_zmachine.h"

#include <cstdio>
#include <cstring>
#include <utility>

namespace
{
const char kAlphabet[3][27] = {
    "abcdefghijklmnopqrstuvwxyz",
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ",
    " \n0123456789.,!?_#'\"/\\-:()",
};

constexpr unsigned kMaxStack = 1024;
constexpr unsigned kMaxFrames = 256;
}

uint8_t ZMachine::byte(uint32_t address) const
{
    return address < m_memory.size() ? m_memory[address] : 0;
}

uint16_t ZMachine::word(uint32_t address) const
{
    return uint16_t(byte(address) << 8 | byte(address + 1));
}

void ZMachine::setByte(uint32_t address, uint8_t value)
{
    if (address >= word(0x0E))
    {
        fail("write to static memory");
        return;
    }
    m_memory[address] = value;
}

void ZMachine::setWord(uint32_t address, uint16_t value)
{
    setByte(address, uint8_t(value >> 8));
    setByte(address + 1, uint8_t(value));
}

uint8_t ZMachine::fetch()
{
    return byte(m_pc++);
}

uint16_t ZMachine::fetchWord()
{
    const uint16_t value = word(m_pc);
    m_pc += 2;
    return value;
}

void ZMachine::fail(const char *text)
{
    if (m_state == State::Halted)
        return;
    char buffer[96];
    std::snprintf(buffer, sizeof(buffer), "%s (pc %05X)", text, unsigned(m_pc));
    m_error = buffer;
    flushOutput();
    m_state = State::Halted;
}

void ZMachine::push(uint16_t value)
{
    if (m_stack.size() >= kMaxStack)
    {
        fail("stack overflow");
        return;
    }
    m_stack.push_back(value);
}

uint16_t ZMachine::pop()
{
    const uint32_t base = m_frames.empty() ? 0 : m_frames.back().stackBase;
    if (m_stack.size() <= base)
    {
        fail("stack underflow");
        return 0;
    }
    const uint16_t value = m_stack.back();
    m_stack.pop_back();
    return value;
}

uint16_t ZMachine::variable(uint8_t index)
{
    if (!index)
        return pop();
    if (index < 16)
    {
        if (m_frames.empty() || index > m_frames.back().localCount)
        {
            fail("bad local variable");
            return 0;
        }
        return m_frames.back().locals[index - 1];
    }
    return word(word(0x0C) + 2 * (index - 16));
}

void ZMachine::setVariable(uint8_t index, uint16_t value)
{
    if (!index)
    {
        push(value);
        return;
    }
    if (index < 16)
    {
        if (m_frames.empty() || index > m_frames.back().localCount)
        {
            fail("bad local variable");
            return;
        }
        m_frames.back().locals[index - 1] = value;
        return;
    }
    setWord(word(0x0C) + 2 * (index - 16), value);
}

// Indirect variable references read and write the stack top in place.
uint16_t ZMachine::peekVariable(uint8_t index)
{
    if (index)
        return variable(index);
    const uint32_t base = m_frames.empty() ? 0 : m_frames.back().stackBase;
    if (m_stack.size() <= base)
    {
        fail("stack underflow");
        return 0;
    }
    return m_stack.back();
}

void ZMachine::pokeVariable(uint8_t index, uint16_t value)
{
    if (index)
    {
        setVariable(index, value);
        return;
    }
    const uint32_t base = m_frames.empty() ? 0 : m_frames.back().stackBase;
    if (m_stack.size() <= base)
    {
        fail("stack underflow");
        return;
    }
    m_stack.back() = value;
}

void ZMachine::store(uint16_t value)
{
    setVariable(fetch(), value);
}

void ZMachine::branch(bool condition)
{
    const uint8_t first = fetch();
    int offset = first & 0x3F;
    if (!(first & 0x40))
    {
        offset = offset << 8 | fetch();
        if (offset & 0x2000)
            offset -= 0x4000;
    }
    if (bool(first & 0x80) != condition)
        return;
    if (offset == 0 || offset == 1)
        ret(uint16_t(offset));
    else
        m_pc = uint32_t(int32_t(m_pc) + offset - 2);
}

void ZMachine::call(uint16_t packed, const uint16_t *args, unsigned argCount, int16_t storeVar)
{
    if (!packed)
    {
        if (storeVar >= 0)
            setVariable(uint8_t(storeVar), 0);
        return;
    }
    if (m_frames.size() >= kMaxFrames)
    {
        fail("call stack overflow");
        return;
    }
    const uint32_t address = 2u * packed;
    if (address >= m_memory.size())
    {
        fail("call outside the story");
        return;
    }
    Frame frame = {};
    frame.returnPc = m_pc;
    frame.storeVar = storeVar;
    frame.stackBase = uint32_t(m_stack.size());
    frame.argCount = uint8_t(argCount);
    frame.localCount = byte(address);
    if (frame.localCount > 15)
    {
        fail("bad routine header");
        return;
    }
    m_pc = address + 1;
    for (unsigned i = 0; i < frame.localCount; ++i)
        frame.locals[i] = fetchWord();
    for (unsigned i = 0; i < argCount && i < frame.localCount; ++i)
        frame.locals[i] = args[i];
    m_frames.push_back(frame);
}

void ZMachine::ret(uint16_t value)
{
    if (m_frames.empty())
    {
        fail("return from the main routine");
        return;
    }
    const Frame frame = m_frames.back();
    m_frames.pop_back();
    m_stack.resize(frame.stackBase);
    m_pc = frame.returnPc;
    if (frame.storeVar >= 0)
        setVariable(uint8_t(frame.storeVar), value);
}

// ----- objects (version 1-3 layout) -----

uint32_t ZMachine::objectAddress(uint16_t object) const
{
    return word(0x0A) + 31 * 2 + (object - 1) * 9u;
}

uint32_t ZMachine::propertyTable(uint16_t object) const
{
    return word(objectAddress(object) + 7);
}

uint32_t ZMachine::firstProperty(uint16_t object) const
{
    const uint32_t table = propertyTable(object);
    return table + 1 + 2 * byte(table);
}

uint32_t ZMachine::propertyAddress(uint16_t object, uint8_t property, unsigned *size) const
{
    for (uint32_t p = firstProperty(object); byte(p); p += 1 + (byte(p) >> 5) + 1)
    {
        const uint8_t header = byte(p);
        if ((header & 31) == property)
        {
            if (size)
                *size = (header >> 5) + 1;
            return p + 1;
        }
        if ((header & 31) < property)
            break;
    }
    return 0;
}

bool ZMachine::attribute(uint16_t object, uint16_t number) const
{
    if (!object || number > 31)
        return false;
    return (byte(objectAddress(object) + number / 8) >> (7 - number % 8)) & 1;
}

void ZMachine::setAttribute(uint16_t object, uint16_t number, bool value)
{
    if (!object || number > 31)
        return;
    const uint32_t address = objectAddress(object) + number / 8;
    const uint8_t mask = uint8_t(0x80 >> (number % 8));
    setByte(address, value ? byte(address) | mask : byte(address) & ~mask);
}

uint8_t ZMachine::parent(uint16_t object) const { return object ? byte(objectAddress(object) + 4) : 0; }
uint8_t ZMachine::sibling(uint16_t object) const { return object ? byte(objectAddress(object) + 5) : 0; }
uint8_t ZMachine::child(uint16_t object) const { return object ? byte(objectAddress(object) + 6) : 0; }
void ZMachine::setParent(uint16_t object, uint8_t value) { setByte(objectAddress(object) + 4, value); }
void ZMachine::setSibling(uint16_t object, uint8_t value) { setByte(objectAddress(object) + 5, value); }
void ZMachine::setChild(uint16_t object, uint8_t value) { setByte(objectAddress(object) + 6, value); }

void ZMachine::removeObject(uint16_t object)
{
    const uint8_t owner = parent(object);
    if (!owner)
        return;
    if (child(owner) == object)
    {
        setChild(owner, sibling(object));
    }
    else
    {
        uint8_t previous = child(owner);
        while (previous && sibling(previous) != object)
            previous = sibling(previous);
        if (previous)
            setSibling(previous, sibling(object));
    }
    setParent(object, 0);
    setSibling(object, 0);
}

void ZMachine::insertObject(uint16_t object, uint16_t destination)
{
    if (!object || !destination)
        return;
    removeObject(object);
    setSibling(object, child(destination));
    setChild(destination, uint8_t(object));
    setParent(object, uint8_t(destination));
}

// ----- text -----

uint32_t ZMachine::decodeText(uint32_t address, std::string &out, bool abbreviation) const
{
    int alphabet = 0;
    int abbreviationBank = -1;
    int escape = -1;    // -1: none, -2: waiting for the high five bits, else the high bits
    for (;;)
    {
        const uint16_t w = word(address);
        address += 2;
        for (int shift = 10; shift >= 0; shift -= 5)
        {
            const int z = (w >> shift) & 31;
            if (escape != -1)
            {
                if (escape == -2)
                {
                    escape = z;
                    continue;
                }
                const int zscii = escape << 5 | z;
                if (zscii >= 32 && zscii < 127)
                    out += char(zscii);
                else if (zscii == 13)
                    out += '\n';
                escape = -1;
                continue;
            }
            if (abbreviationBank >= 0)
            {
                // Abbreviations never nest.
                if (!abbreviation)
                    decodeText(2u * word(word(0x18) + 2 * (32 * abbreviationBank + z)), out, true);
                abbreviationBank = -1;
                continue;
            }
            if (z == 0)
                out += ' ';
            else if (z <= 3)
                abbreviationBank = z - 1;
            else if (z <= 5)
            {
                alphabet = z - 3;
                continue;
            }
            else if (alphabet == 2 && z == 6)
                escape = -2;
            else if (alphabet == 2 && z == 7)
                out += '\n';
            else
                out += kAlphabet[alphabet][z - 6];
            alphabet = 0;
        }
        if ((w & 0x8000) || address >= m_memory.size())
            break;
    }
    return address;
}

void ZMachine::printText(const std::string &text)
{
    for (const char c : text)
        printChar(uint8_t(c));
}

void ZMachine::printChar(uint16_t zscii)
{
    if (zscii == 13)
        zscii = '\n';
    if (zscii != '\n' && (zscii < 32 || zscii > 126))
        return;
    if (!m_tables.empty())
    {
        const uint32_t table = m_tables.back();
        const uint16_t length = word(table);
        setByte(table + 2 + length, uint8_t(zscii == '\n' ? 13 : zscii));
        setWord(table, uint16_t(length + 1));
        return;
    }
    m_output += char(zscii);
}

void ZMachine::printNumber(int16_t value)
{
    char text[8];
    std::snprintf(text, sizeof(text), "%d", int(value));
    for (const char *c = text; *c; ++c)
        printChar(uint8_t(*c));
}

void ZMachine::objectName(uint16_t object, std::string &out) const
{
    if (object && byte(propertyTable(object)))
        decodeText(propertyTable(object) + 1, out);
}

void ZMachine::flushOutput()
{
    if (!m_output.empty() && m_io)
        m_io->print(m_output.data(), m_output.size());
    m_output.clear();
}

void ZMachine::showStatus()
{
    if (!m_io)
        return;
    std::string location;
    objectName(word(word(0x0C)), location);
    const bool timeGame = (byte(0x01) & 0x02) != 0;
    m_io->statusLine(location.c_str(), int16_t(word(word(0x0C) + 2)), int16_t(word(word(0x0C) + 4)), timeGame);
}

// ----- input -----

void ZMachine::encodeWord(const char *text, size_t length, uint8_t out[4]) const
{
    uint8_t z[6];
    unsigned count = 0;
    const unsigned limit = 6;
    for (size_t i = 0; i < length && count < limit; ++i)
    {
        const char c = text[i];
        const char *a0 = std::strchr(kAlphabet[0], c);
        const char *a2 = c ? std::strchr(kAlphabet[2] + 2, c) : nullptr;
        if (c && a0)
        {
            z[count++] = uint8_t(6 + (a0 - kAlphabet[0]));
        }
        else if (a2)
        {
            z[count++] = 5;
            if (count < limit)
                z[count++] = uint8_t(8 + (a2 - (kAlphabet[2] + 2)));
        }
        else
        {
            const uint8_t zscii = uint8_t(c);
            const uint8_t sequence[4] = {5, 6, uint8_t(zscii >> 5), uint8_t(zscii & 31)};
            for (unsigned j = 0; j < 4 && count < limit; ++j)
                z[count++] = sequence[j];
        }
    }
    while (count < limit)
        z[count++] = 5;
    const uint16_t w0 = uint16_t(z[0] << 10 | z[1] << 5 | z[2]);
    const uint16_t w1 = uint16_t(z[3] << 10 | z[4] << 5 | z[5] | 0x8000);
    out[0] = uint8_t(w0 >> 8);
    out[1] = uint8_t(w0);
    out[2] = uint8_t(w1 >> 8);
    out[3] = uint8_t(w1);
}

uint16_t ZMachine::lookup(const char *text, size_t length) const
{
    uint8_t key[4];
    encodeWord(text, length, key);
    const uint32_t dictionary = word(0x08);
    const uint32_t separators = byte(dictionary);
    const uint32_t entryLength = byte(dictionary + 1 + separators);
    const int16_t entryCount = int16_t(word(dictionary + 2 + separators));
    const uint32_t entries = dictionary + 4 + separators;
    const auto compare = [&](uint32_t entry) { return std::memcmp(&m_memory[entry], key, 4); };
    if (entryCount < 0 || entries + uint32_t(entryCount) * entryLength > m_memory.size())
    {
        const int count = entryCount < 0 ? -entryCount : entryCount;
        for (int i = 0; i < count; ++i)
        {
            const uint32_t entry = entries + uint32_t(i) * entryLength;
            if (entry + 4 <= m_memory.size() && !compare(entry))
                return uint16_t(entry);
        }
        return 0;
    }
    int low = 0, high = entryCount - 1;
    while (low <= high)
    {
        const int middle = (low + high) / 2;
        const uint32_t entry = entries + uint32_t(middle) * entryLength;
        const int order = compare(entry);
        if (!order)
            return uint16_t(entry);
        if (order < 0)
            low = middle + 1;
        else
            high = middle - 1;
    }
    return 0;
}

void ZMachine::tokenize(uint32_t textBuffer, uint32_t parseBuffer)
{
    const uint32_t dictionary = word(0x08);
    const uint8_t separatorCount = byte(dictionary);
    const auto isSeparator = [&](char c) {
        for (uint8_t i = 0; i < separatorCount; ++i)
            if (byte(dictionary + 1 + i) == uint8_t(c))
                return true;
        return false;
    };
    const uint8_t maxWords = byte(parseBuffer);
    uint8_t words = 0;
    const uint32_t start = textBuffer + 1;
    uint32_t p = start;
    while (byte(p) && words < maxWords)
    {
        const char c = char(byte(p));
        if (c == ' ')
        {
            ++p;
            continue;
        }
        uint32_t end = p + 1;
        if (!isSeparator(c))
            while (byte(end) && byte(end) != ' ' && !isSeparator(char(byte(end))))
                ++end;
        const uint32_t length = end - p;
        const uint16_t entry = lookup(reinterpret_cast<const char *>(&m_memory[p]), length);
        const uint32_t record = parseBuffer + 2 + 4u * words;
        setWord(record, entry);
        setByte(record + 2, uint8_t(length));
        setByte(record + 3, uint8_t(p - textBuffer));
        ++words;
        p = end;
    }
    setByte(parseBuffer + 1, words);
}

void ZMachine::input(const char *line)
{
    if (m_state != State::NeedInput)
        return;
    const uint8_t capacity = byte(m_textBuffer);
    const unsigned limit = capacity > 1 ? capacity - 1u : 0u;
    unsigned length = 0;
    for (const char *c = line; *c && length < limit; ++c)
    {
        char value = *c;
        if (value >= 'A' && value <= 'Z')
            value = char(value - 'A' + 'a');
        if (value < 32 || value > 126)
            continue;
        setByte(m_textBuffer + 1 + length, uint8_t(value));
        ++length;
    }
    setByte(m_textBuffer + 1 + length, 0);
    tokenize(m_textBuffer, m_parseBuffer);
    if (m_state == State::NeedInput)
        m_state = State::Running;
}

// ----- misc -----

uint16_t ZMachine::random(int16_t range)
{
    if (range < 0)
    {
        m_randomFixed = true;
        m_randomCounter = 0;
        m_randomRange = uint16_t(-range);
        m_seed = uint32_t(-range);
        return 0;
    }
    if (range == 0)
    {
        m_randomFixed = false;
        m_seed = m_seed * 1103515245u + 12345u;
        return 0;
    }
    if (m_randomFixed && m_randomRange < 1000)
    {
        m_randomCounter = uint16_t(m_randomCounter % m_randomRange + 1);
        return uint16_t((m_randomCounter - 1) % range + 1);
    }
    m_seed ^= m_seed << 13;
    m_seed ^= m_seed >> 17;
    m_seed ^= m_seed << 5;
    return uint16_t(m_seed % uint32_t(range) + 1);
}

void ZMachine::restart()
{
    const uint8_t flags2 = byte(0x11) & 0x03;
    std::memcpy(m_memory.data(), m_original.data(), word(0x0E));
    m_memory[0x11] = uint8_t((m_memory[0x11] & ~0x03) | flags2);
    m_memory[0x01] &= uint8_t(~0x30);   // status line available, no split screen
    m_stack.clear();
    m_frames.clear();
    m_tables.clear();
    m_pc = word(0x06);
}

// Saved game image: "OBZS", format version, the story's release/serial/checksum, then
// the PC, dynamic memory, evaluation stack and call frames, little-endian.
namespace
{
constexpr uint8_t kSaveVersion = 1;

void putU8(std::vector<uint8_t> &out, uint8_t value)
{
    out.push_back(value);
}

void putU16(std::vector<uint8_t> &out, uint16_t value)
{
    out.push_back(uint8_t(value));
    out.push_back(uint8_t(value >> 8));
}

void putU32(std::vector<uint8_t> &out, uint32_t value)
{
    putU16(out, uint16_t(value));
    putU16(out, uint16_t(value >> 16));
}

class SaveReader
{
public:
    SaveReader(const std::vector<uint8_t> &data) : m_data(data) {}

    bool u8(uint8_t &value)
    {
        if (m_offset + 1 > m_data.size())
            return false;
        value = m_data[m_offset++];
        return true;
    }

    bool u16(uint16_t &value)
    {
        uint8_t low, high;
        if (!u8(low) || !u8(high))
            return false;
        value = uint16_t(low | high << 8);
        return true;
    }

    bool u32(uint32_t &value)
    {
        uint16_t low, high;
        if (!u16(low) || !u16(high))
            return false;
        value = uint32_t(low) | uint32_t(high) << 16;
        return true;
    }

    bool bytes(uint8_t *out, size_t size)
    {
        if (m_offset + size > m_data.size())
            return false;
        std::memcpy(out, m_data.data() + m_offset, size);
        m_offset += size;
        return true;
    }

private:
    const std::vector<uint8_t> &m_data;
    size_t m_offset = 0;
};
}

bool ZMachine::save()
{
    if (!m_io)
        return false;
    std::vector<uint8_t> data;
    const uint32_t dynamicSize = word(0x0E);
    data.reserve(64 + dynamicSize + 2 * m_stack.size() + sizeof(Frame) * m_frames.size());
    data.insert(data.end(), {'O', 'B', 'Z', 'S'});
    putU8(data, kSaveVersion);
    putU16(data, word(0x02));
    data.insert(data.end(), m_memory.begin() + 0x12, m_memory.begin() + 0x18);
    putU16(data, word(0x1C));
    putU32(data, m_pc);
    putU32(data, dynamicSize);
    data.insert(data.end(), m_memory.begin(), m_memory.begin() + dynamicSize);
    putU32(data, uint32_t(m_stack.size()));
    for (const uint16_t value : m_stack)
        putU16(data, value);
    putU32(data, uint32_t(m_frames.size()));
    for (const Frame &frame : m_frames)
    {
        putU32(data, frame.returnPc);
        for (const uint16_t local : frame.locals)
            putU16(data, local);
        putU8(data, frame.localCount);
        putU8(data, frame.argCount);
        putU16(data, uint16_t(frame.storeVar));
        putU32(data, frame.stackBase);
    }
    return m_io->saveGame(data.data(), data.size());
}

bool ZMachine::restore()
{
    std::vector<uint8_t> data;
    if (!m_io || !m_io->restoreGame(data))
        return false;

    SaveReader reader(data);
    uint8_t magic[4], version, serial[6];
    uint16_t release, checksum;
    uint32_t pc, dynamicSize, stackSize, frameCount;
    if (!reader.bytes(magic, 4) || std::memcmp(magic, "OBZS", 4) || !reader.u8(version) || version != kSaveVersion)
        return false;
    if (!reader.u16(release) || !reader.bytes(serial, 6) || !reader.u16(checksum))
        return false;
    if (release != word(0x02) || std::memcmp(serial, &m_memory[0x12], 6) || checksum != word(0x1C))
        return false;
    if (!reader.u32(pc) || pc >= m_memory.size() || !reader.u32(dynamicSize) || dynamicSize != word(0x0E))
        return false;

    std::vector<uint8_t> dynamic(dynamicSize);
    if (!reader.bytes(dynamic.data(), dynamicSize) || !reader.u32(stackSize) || stackSize > kMaxStack)
        return false;
    std::vector<uint16_t> stack(stackSize);
    for (uint16_t &value : stack)
        if (!reader.u16(value))
            return false;
    if (!reader.u32(frameCount) || frameCount > kMaxFrames)
        return false;
    std::vector<Frame> frames(frameCount);
    uint32_t previousBase = 0;
    for (Frame &frame : frames)
    {
        uint16_t storeVar;
        if (!reader.u32(frame.returnPc))
            return false;
        for (uint16_t &local : frame.locals)
            if (!reader.u16(local))
                return false;
        if (!reader.u8(frame.localCount) || !reader.u8(frame.argCount) || !reader.u16(storeVar) || !reader.u32(frame.stackBase))
            return false;
        frame.storeVar = int16_t(storeVar);
        if (frame.localCount > 15 || frame.returnPc >= m_memory.size() || frame.stackBase < previousBase || frame.stackBase > stackSize)
            return false;
        previousBase = frame.stackBase;
    }

    const uint8_t flags2 = byte(0x11) & 0x03;
    std::memcpy(m_memory.data(), dynamic.data(), dynamicSize);
    m_memory[0x11] = uint8_t((m_memory[0x11] & ~0x03) | flags2);
    m_stack = std::move(stack);
    m_frames = std::move(frames);
    m_pc = pc;
    return true;
}

bool ZMachine::verify() const
{
    uint32_t length = 2u * word(0x1A);
    if (length > m_original.size())
        length = uint32_t(m_original.size());
    uint16_t sum = 0;
    for (uint32_t i = 0x40; i < length; ++i)
        sum = uint16_t(sum + m_original[i]);
    return sum == word(0x1C);
}

bool ZMachine::load(const uint8_t *story, size_t size, ZMachineIo *io, uint32_t seed)
{
    m_error.clear();
    m_state = State::Halted;
    if (!story || size < 64 || story[0] != 3)
    {
        m_error = "unsupported story file";
        return false;
    }
    m_original.assign(story, story + size);
    m_memory = m_original;
    m_io = io;
    m_seed = seed ? seed : 1;
    m_randomFixed = false;
    m_output.clear();
    restart();
    m_state = State::Running;
    return true;
}

ZMachine::State ZMachine::run(unsigned budget)
{
    while (m_state == State::Running && budget--)
        step();
    if (m_state != State::Running)
        flushOutput();
    return m_state;
}

void ZMachine::step()
{
    const uint8_t opcode = fetch();
    uint16_t operands[8];
    unsigned count = 0;
    unsigned number;
    enum { Op0, Op1, Op2, OpVar } kind;

    const auto readOperand = [&](unsigned type) {
        switch (type)
        {
        case 0: operands[count++] = fetchWord(); break;
        case 1: operands[count++] = fetch(); break;
        case 2: operands[count++] = variable(fetch()); break;
        default: break;
        }
    };

    if (opcode < 0x80)
    {
        kind = Op2;
        number = opcode & 0x1F;
        readOperand(opcode & 0x40 ? 2 : 1);
        readOperand(opcode & 0x20 ? 2 : 1);
    }
    else if (opcode < 0xC0)
    {
        number = opcode & 0x0F;
        const unsigned type = (opcode >> 4) & 3;
        kind = type == 3 ? Op0 : Op1;
        readOperand(type);
    }
    else
    {
        kind = (opcode & 0x20) ? OpVar : Op2;
        number = opcode & 0x1F;
        const uint8_t types = fetch();
        for (int shift = 6; shift >= 0; shift -= 2)
        {
            const unsigned type = (types >> shift) & 3;
            if (type == 3)
                break;
            readOperand(type);
        }
    }
    if (m_state != State::Running)
        return;

    const int16_t a = int16_t(count > 0 ? operands[0] : 0);
    const int16_t b = int16_t(count > 1 ? operands[1] : 0);
    const uint16_t ua = count > 0 ? operands[0] : 0;
    const uint16_t ub = count > 1 ? operands[1] : 0;

    switch (kind)
    {
    case Op2:
        switch (number)
        {
        case 0x01: // je
        {
            bool equal = false;
            for (unsigned i = 1; i < count; ++i)
                equal = equal || operands[i] == ua;
            branch(equal);
            break;
        }
        case 0x02: branch(a < b); break;                       // jl
        case 0x03: branch(a > b); break;                       // jg
        case 0x04:                                             // dec_chk
        {
            const int16_t value = int16_t(peekVariable(uint8_t(ua)) - 1);
            pokeVariable(uint8_t(ua), uint16_t(value));
            branch(value < b);
            break;
        }
        case 0x05:                                             // inc_chk
        {
            const int16_t value = int16_t(peekVariable(uint8_t(ua)) + 1);
            pokeVariable(uint8_t(ua), uint16_t(value));
            branch(value > b);
            break;
        }
        case 0x06: branch(parent(ua) == ub); break;            // jin
        case 0x07: branch((ua & ub) == ub); break;             // test
        case 0x08: store(ua | ub); break;                      // or
        case 0x09: store(ua & ub); break;                      // and
        case 0x0A: branch(attribute(ua, ub)); break;           // test_attr
        case 0x0B: setAttribute(ua, ub, true); break;          // set_attr
        case 0x0C: setAttribute(ua, ub, false); break;         // clear_attr
        case 0x0D: pokeVariable(uint8_t(ua), ub); break;       // store
        case 0x0E: insertObject(ua, ub); break;                // insert_obj
        case 0x0F: store(word(uint16_t(ua + 2 * ub))); break;  // loadw
        case 0x10: store(byte(uint16_t(ua + ub))); break;      // loadb
        case 0x11:                                             // get_prop
        {
            unsigned size = 0;
            const uint32_t address = propertyAddress(ua, uint8_t(ub), &size);
            if (!address)
                store(word(word(0x0A) + 2 * (ub - 1)));
            else
                store(size == 1 ? byte(address) : word(address));
            break;
        }
        case 0x12: store(uint16_t(propertyAddress(ua, uint8_t(ub), nullptr))); break; // get_prop_addr
        case 0x13:                                             // get_next_prop
        {
            uint32_t p;
            if (!ub)
            {
                p = firstProperty(ua);
            }
            else
            {
                unsigned size = 0;
                const uint32_t address = propertyAddress(ua, uint8_t(ub), &size);
                if (!address)
                {
                    fail("get_next_prop on a missing property");
                    break;
                }
                p = address + size;
            }
            store(byte(p) & 31);
            break;
        }
        case 0x14: store(uint16_t(a + b)); break;              // add
        case 0x15: store(uint16_t(a - b)); break;              // sub
        case 0x16: store(uint16_t(a * b)); break;              // mul
        case 0x17:                                             // div
            if (!b)
                fail("division by zero");
            else
                store(uint16_t(a / b));
            break;
        case 0x18:                                             // mod
            if (!b)
                fail("division by zero");
            else
                store(uint16_t(a % b));
            break;
        default:
            fail("unknown 2OP instruction");
            break;
        }
        break;

    case Op1:
        switch (number)
        {
        case 0x0: branch(ua == 0); break;                      // jz
        case 0x1: { const uint8_t s = sibling(ua); store(s); branch(s != 0); break; } // get_sibling
        case 0x2: { const uint8_t c = child(ua); store(c); branch(c != 0); break; }   // get_child
        case 0x3: store(parent(ua)); break;                    // get_parent
        case 0x4: store(ua ? uint16_t((byte(ua - 1u) >> 5) + 1) : 0); break; // get_prop_len
        case 0x5: pokeVariable(uint8_t(ua), uint16_t(peekVariable(uint8_t(ua)) + 1)); break; // inc
        case 0x6: pokeVariable(uint8_t(ua), uint16_t(peekVariable(uint8_t(ua)) - 1)); break; // dec
        case 0x7: { std::string text; decodeText(ua, text); printText(text); break; }        // print_addr
        case 0x9: removeObject(ua); break;                     // remove_obj
        case 0xA: { std::string text; objectName(ua, text); printText(text); break; }        // print_obj
        case 0xB: ret(ua); break;                              // ret
        case 0xC: m_pc = uint32_t(int32_t(m_pc) + a - 2); break; // jump
        case 0xD: { std::string text; decodeText(2u * ua, text); printText(text); break; }   // print_paddr
        case 0xE: store(peekVariable(uint8_t(ua))); break;     // load
        case 0xF: store(uint16_t(~ua)); break;                 // not
        default:
            fail("unknown 1OP instruction");
            break;
        }
        break;

    case Op0:
        switch (number)
        {
        case 0x0: ret(1); break;                               // rtrue
        case 0x1: ret(0); break;                               // rfalse
        case 0x2:                                              // print
        case 0x3:                                              // print_ret
        {
            std::string text;
            m_pc = decodeText(m_pc, text);
            printText(text);
            if (number == 0x3)
            {
                printChar('\n');
                ret(1);
            }
            break;
        }
        case 0x4: break;                                       // nop
        case 0x5: branch(save()); break;                       // save
        case 0x6:                                              // restore
            if (restore())
                branch(true);
            else
                branch(false);
            break;
        case 0x7: restart(); break;                            // restart
        case 0x8: ret(pop()); break;                           // ret_popped
        case 0x9: pop(); break;                                // pop
        case 0xA: flushOutput(); m_state = State::Halted; break; // quit
        case 0xB: printChar('\n'); break;                      // new_line
        case 0xC: showStatus(); break;                         // show_status
        case 0xD: branch(verify()); break;                     // verify
        default:
            fail("unknown 0OP instruction");
            break;
        }
        break;

    case OpVar:
        switch (number)
        {
        case 0x00:                                             // call
            call(ua, operands + 1, count ? count - 1 : 0, int16_t(fetch()));
            break;
        case 0x01: setWord(uint16_t(ua + 2 * ub), count > 2 ? operands[2] : 0); break;  // storew
        case 0x02: setByte(uint16_t(ua + ub), uint8_t(count > 2 ? operands[2] : 0)); break; // storeb
        case 0x03:                                             // put_prop
        {
            unsigned size = 0;
            const uint32_t address = propertyAddress(ua, uint8_t(ub), &size);
            if (!address)
                fail("put_prop on a missing property");
            else if (size == 1)
                setByte(address, uint8_t(operands[2]));
            else
                setWord(address, operands[2]);
            break;
        }
        case 0x04:                                             // sread
            showStatus();
            flushOutput();
            m_textBuffer = ua;
            m_parseBuffer = ub;
            m_state = State::NeedInput;
            break;
        case 0x05: printChar(ua); break;                       // print_char
        case 0x06: printNumber(a); break;                      // print_num
        case 0x07: store(random(a)); break;                    // random
        case 0x08: push(ua); break;                            // push
        case 0x09: pokeVariable(uint8_t(ua), pop()); break;    // pull
        case 0x0A: break;                                      // split_window
        case 0x0B: break;                                      // set_window
        case 0x13:                                             // output_stream
            if (a == 3 && m_tables.size() < 16)
            {
                setWord(ub, 0);
                m_tables.push_back(ub);
            }
            else if (a == -3 && !m_tables.empty())
            {
                m_tables.pop_back();
            }
            break;
        case 0x14: break;                                      // input_stream
        case 0x15: break;                                      // sound_effect
        default:
            fail("unknown VAR instruction");
            break;
        }
        break;
    }
}
