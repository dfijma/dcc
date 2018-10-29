#pragma once

static const int SLOTS = 2; // slots in a buffer
static const int MAX_CMDS_PACKET = 3; // maximum number of cmds in a encoded packet
static const int MAX_CMD_ENCODED_SIZE = 10; // the max size of a single encodeded cmd
static const int MAX_CMD_SIZE = 6; // the max size ofa single non-encoded cmd

//// abstract bitstream, representing a sequence of encoded DCC cmds

class BitStream {
  public:
    virtual int length() = 0; // in bits
    virtual boolean getBit(int bit) = 0; 
};

#ifdef TESTING
// decode and print such a DCC bit stream
void decodePacket(BitStream& s);

//// trivial implementation backed by a byte buffer, used with testing
class TestBitStream : public BitStream {
  public:
    TestBitStream() { buffered = 0; }
    boolean getBit(int bit) { return buffer[bit]; }
    int length() { return buffered; }
    void add(boolean b) { buffer[buffered++] = b; }
    void reset() { buffered = 0; }
  private:
    boolean buffer[MAX_CMDS_PACKET*MAX_CMD_ENCODED_SIZE*8]; // in bits
    int buffered;
};
#endif

//// a Packet contains one or more encoded DCC cmds

class Packet : public BitStream {
  public:
    Packet() { reset(); } // initially empty
    boolean getBit(int bit);
    int length() { return 8*buffered; }
    void reset() { buffered = 0; }
    Packet& withIdleCmd();
    Packet& withThrottleCmd(int address, byte speed /* 0..126 */, boolean forward, boolean emergencyStop);
    Packet& withF1Cmd(int address, byte FLF4F3F2F1);
    Packet& withF2LowCmd(int adress, byte F8F7F6F5);
    Packet& withF2HighCmd(int address, byte F12F11F10F9);
  private:
    byte buffer[MAX_CMDS_PACKET*MAX_CMD_ENCODED_SIZE]; // room for max 3 packets of max length 10 bytes encoded
    byte buffered; // number of byte in the packet

    void loadCmd(byte in[], byte nBytes);
    byte loadAddress(byte* buffer, int address);
};

//// A Slot is a combination of an active packet (to be modulated) and an updatable packet (to latch new cmds)

class Slot : public BitStream {
  public:
    Slot() {
      activePacket = &packets[0];
      updatePacket = &packets[1];
    } 
    Packet& update() { return *updatePacket; } // updatable packet for new cmds
    void flip(); // flip update packet into active
    boolean getBit(int bit) { return activePacket->getBit(bit); }
    int length() { return activePacket->length(); }
  private:
    Packet packets[2]; // two packets: the active packet and the packet to be updated, both initially empty
    Packet *activePacket;
    Packet *updatePacket; 
};

//// A Buffer is set of slots to be modulated 

// Unitially, the active packet in each slot is an idle cmd and the update packet is empty.
// After initializing a buffer, we can thus immediately start cycling through all bits using
// nextBit(), and modulate an endless loop of idle cmds, so that we at least have power.

class Buffer {
  public:
    Buffer();
    Slot& slot(byte s);
    bool nextBit(); // 
#ifdef TESTING
    bool test(); // collect all bits and try to decode
#endif
  
  private:
    byte brk; 
    Slot slots[SLOTS]; // some SLOTS, initially active packet is an idle cmd and update packet is empty
    byte currentSlot;
    byte currentBit;
};




