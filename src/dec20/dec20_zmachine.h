#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// Z-machine interpreter for version 3 story files (the terminal's Zork).
// The host feeds input lines and receives the output text through ZMachineIo.
class ZMachineIo
{
public:
    virtual ~ZMachineIo() = default;
    virtual void print(const char *text, size_t length) = 0;
    // Version 3 status line: location name and score/moves, or hours/minutes
    // for time games.
    virtual void statusLine(const char *location, int scoreOrHours, int movesOrMinutes, bool timeGame) = 0;
    // Saved game storage; the image is opaque to the host.
    virtual bool saveGame(const uint8_t *data, size_t size) = 0;
    virtual bool restoreGame(std::vector<uint8_t> &data) = 0;
};

class ZMachine
{
public:
    enum class State
    {
        Running,
        NeedInput,
        Halted,
    };

    bool load(const uint8_t *story, size_t size, ZMachineIo *io, uint32_t seed);
    // Runs until the game asks for a line, quits, or `budget` instructions ran.
    State run(unsigned budget);
    void input(const char *line);
    State state() const { return m_state; }
    const char *error() const { return m_error.c_str(); }

private:
    struct Frame
    {
        uint32_t returnPc;
        uint16_t locals[15];
        uint8_t localCount;
        uint8_t argCount;
        int16_t storeVar;   // -1: discard the result
        uint32_t stackBase;
    };

    uint8_t byte(uint32_t address) const;
    uint16_t word(uint32_t address) const;
    void setByte(uint32_t address, uint8_t value);
    void setWord(uint32_t address, uint16_t value);
    uint8_t fetch();
    uint16_t fetchWord();

    uint16_t variable(uint8_t index);
    void setVariable(uint8_t index, uint16_t value);
    uint16_t peekVariable(uint8_t index);
    void pokeVariable(uint8_t index, uint16_t value);
    void push(uint16_t value);
    uint16_t pop();

    void step();
    void store(uint16_t value);
    void branch(bool condition);
    void call(uint16_t packed, const uint16_t *args, unsigned argCount, int16_t storeVar);
    void ret(uint16_t value);
    void fail(const char *text);

    uint32_t objectAddress(uint16_t object) const;
    uint32_t propertyTable(uint16_t object) const;
    uint32_t firstProperty(uint16_t object) const;
    uint32_t propertyAddress(uint16_t object, uint8_t property, unsigned *size) const;
    bool attribute(uint16_t object, uint16_t number) const;
    void setAttribute(uint16_t object, uint16_t number, bool value);
    uint8_t parent(uint16_t object) const;
    uint8_t sibling(uint16_t object) const;
    uint8_t child(uint16_t object) const;
    void setParent(uint16_t object, uint8_t value);
    void setSibling(uint16_t object, uint8_t value);
    void setChild(uint16_t object, uint8_t value);
    void removeObject(uint16_t object);
    void insertObject(uint16_t object, uint16_t destination);

    uint32_t decodeText(uint32_t address, std::string &out, bool abbreviation = false) const;
    void printText(const std::string &text);
    void printChar(uint16_t zscii);
    void printNumber(int16_t value);
    void objectName(uint16_t object, std::string &out) const;
    void showStatus();
    void flushOutput();

    void encodeWord(const char *text, size_t length, uint8_t out[4]) const;
    uint16_t lookup(const char *text, size_t length) const;
    void tokenize(uint32_t textBuffer, uint32_t parseBuffer);

    uint16_t random(int16_t range);
    void restart();
    bool save();
    bool restore();
    bool verify() const;

    std::vector<uint8_t> m_original;
    std::vector<uint8_t> m_memory;
    std::vector<uint16_t> m_stack;
    std::vector<Frame> m_frames;
    std::vector<uint32_t> m_tables;   // output stream 3 table addresses
    std::string m_output;
    std::string m_error;
    ZMachineIo *m_io = nullptr;
    uint32_t m_pc = 0;
    uint32_t m_textBuffer = 0, m_parseBuffer = 0;
    uint32_t m_seed = 1;
    bool m_randomFixed = false;
    uint16_t m_randomCounter = 0, m_randomRange = 0;
    State m_state = State::Halted;
};
