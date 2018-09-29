
#include "Arduino.h"
#include "Config.h"
#include "Buffer.h"

//// decoding for testing purposes

#ifdef TESTING

static byte decodeCmd(BitStream& p, byte startAt) {
  // decode a single cmd from the bitstream, returning its length in the original bitstream
  byte buffer[MAX_CMD_SIZE];
  byte nBytes = 0;
  int currentBit=startAt;
  // count the preamble bits
  int preambleBits = 0;
  while (currentBit < p.length() && p.getBit(currentBit)) {
    preambleBits++;
    currentBit++;
  }
  Serial.print("starting at: "); Serial.print(startAt); Serial.print(" preamble: "); Serial.println(preambleBits); 
  if (preambleBits < 14) {
    Serial.println("not a full cmd");
    return currentBit;
  }
  // next is either the stop bit for the packet (1) or the start bit for the next databyte (0)
  // read data bytes until we see stop bit
  while (!p.getBit(currentBit)) {
    // skip the data start bit
    currentBit++;
    // read 8 data bits
    buffer[nBytes] = 0;
    for (int j=0; j<8; j++) {
      buffer[nBytes] *= 2;
      if (p.getBit(currentBit)) {
        buffer[nBytes] += 1;
      }
      currentBit++;
    }
    nBytes++;
  }
  // skip the stop bit
  currentBit++; 

  for (int j=0; j<nBytes; ++j) {
    Serial.print("byte: "); Serial.print(buffer[j], BIN); Serial.print(" "); Serial.println(buffer[j]);
  }

  byte cs = 0;
  for (int j = 0; j < nBytes-1; j++) {      // XOR remaining bytes into checksum byte
    cs ^= buffer[j];
  }
  Serial.print("expected cs: "); Serial.print(cs); Serial.print(", actual: "); Serial.println(buffer[nBytes-1]);

  int address;
  int parseByte;
  if ((buffer[0] & 0xFF) == 0xFF) {
    Serial.print("adress: IDLE with databyte");
    parseByte=1; 
    Serial.println(buffer[parseByte]);
  } else {
    if ((buffer[0] & 0xC0) == 0xC0) {
      address = (buffer[0] & 0x3F) * 256 + buffer[1];
      parseByte = 2;
    } else {
      address = (buffer[0]);
      parseByte = 1;
    }
    Serial.print("address: "); Serial.println(address);
    if (buffer[parseByte] == 0x3F) {
      Serial.print("cmd: 128 speed step: ");
      parseByte += 1;
      if ((buffer[parseByte] & 0b10000000) == 0b10000000) {
        Serial.print("forwards ");
      } else {
        Serial.print("backwards ");
      }
      byte speed = buffer[parseByte] & 0b01111111;
      if (speed == 1) {
        Serial.println("emergency stop");
      } else {
        if (speed > 0) speed = speed-1;
        Serial.println(speed);
      }
    } else if ((buffer[parseByte] & 0b11100000) == 0b10000000) {
      Serial.print("cmd: function group 1: FL-F4-F3-F2-F1="); 
      Serial.println(buffer[parseByte] & 0b00011111, BIN);
    }
  }  
  
  return currentBit;  
}

void decodePacket(BitStream& s) {
  Serial.println("start decoding packet:");
  int cmds = 0;
  int next = 0;
  while (next < s.length()) {
    next = decodeCmd(s, next);
    cmds++;
  }
  Serial.print("end decoding, saw: "); Serial.print(cmds); Serial.println(" cmds (including possible last empty cmd)");
}

#endif

//// Packet

static const byte mask[] = {0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01}; // precalculated masks to get a bit

boolean Packet::getBit(int bit) {
  return buffer[bit / 8] & mask[bit % 8];
}

void Packet::loadCmd(byte in[], byte nBytes) {
  // this could be generalized, but we know a DCC cmd is always 3, 4, 5 or 6 bytes, so we unfold a bit
  byte *out = &buffer[buffered];
  in[nBytes] = in[0]; // copy first byte into what will become the checksum byte
  for (int i = 1; i < nBytes; i++) {      // XOR remaining bytes into checksum byte
    in[nBytes] ^= in[i];
  }
  nBytes++;                              // increment number of bytes in packet to include checksum byte

  // TODO: we could do with a somewhat smaller preamble (14 minimum)
  out[0] = 0xFF;                       // first 8 bits of 22-byte preamble
  out[1] = 0xFF;                       // second 8 bits of 22-byte preamble
  out[2] = 0xFC + bitRead(in[0], 7);   // last 6 bits of 22-byte preamble + data start bit + b[0], bit 7
  out[3] = in[0] << 1;                 // in[0], bits 6-0 + data start bit
  out[4] = in[1];                      // in[1], all bits
  out[5] = in[2] >> 1;                 // in[2], bits 7-1
  out[6] = in[2] << 7;                 // in[2], bit 0

  if (nBytes == 3) {
    out[6] = out[6] | 0x7F; // + 7 padding/stop bits
    buffered += 7;
  } else {
    out[6] += in[3] >> 2;              // in[3], bits 7-2
    out[7] = in[3] << 6;               // in[3], bit 1-0
    if (nBytes == 4) {
      out[7] = out[7] | 0x3F; // + 6 padding/stop bits
      buffered += 8;
    } else {
      out[7] += in[4] >> 3;            // in[4], bits 7-3
      out[8] = in[4] << 5;             // in[4], bits 2-0
      if (nBytes == 5) {
        out[8] = out[8] | 0x1F; // + 5 padding/stop bits
        buffered += 9;
      } else {
        out[8] += in[5] >> 4;          // in[5], bits 7-4
        out[9] = (in[5] << 4) | 0x0F ; // in[5], bits 3-0 + 4 padding/stop bits
        buffered += 10;
      } 
    } 
  } 
}

Packet& Packet::withIdleCmd() {
  static byte idlePacket[3] = {0xFF, 0x00, 0x00}; // length 2 + room for checksum
  loadCmd(idlePacket, 2);
  return *this;
}

byte Packet::loadAddress(byte* buffer, int address) {
  byte res = 0;
  if (address > 127) {
    buffer[res++] = highByte(address) | 0xC0;    // convert train number into a two-byte address
  }
  buffer[res++] = lowByte(address);
  return res;
}

Packet& Packet::withF1Cmd(int address, byte FLF4F3F2F1) {
  // five least-significant bits control FL-F4-F3-F2-F1 or function group One
  static byte buffer[4]; // 1 or 2 bytes for address, 1 byte for actual command, 1 byte for checksum
    // high byte cab, low byte cab, 1100 f4f3f2f1 (from byte b)
  int nB = loadAddress(buffer, address);
  buffer[nB++] = 0x80 | FLF4F3F2F1; // 100x xxxx is function group one
  loadCmd(buffer, nB);
  return *this;
}

Packet& Packet::withThrottleCmd(int address, byte speed /* 0..126 */, boolean forward, boolean emergencyStop) {
  static byte buffer[5]; // max 5 bytes, including checksum byte
  byte nB = loadAddress(buffer, address);
  buffer[nB++] = 0x3F;                      // 128-step speed control byte
  if (emergencyStop) {
    buffer[nB++] = 1; // emergency stop
  } else {
    // max speed is 126, but speed codes range from 2-127 (0=stop, 1=emergency stop)
    // so map 0, 1, 2 .. 126 -> to 0, 2, 3, .. 127 and set high bit iff forward
    buffer[nB++] = (speed + (speed > 0 ? 1 : 0)) | (forward ? 0x80 : 0x00);
  }
  loadCmd(buffer, nB);
  return *this;
}

#ifdef TESTING
void testPacket() {
  Packet p;
  p.withF1Cmd(998, 0b00010101).
    withThrottleCmd(999, 99, true, false).
    withIdleCmd();
  decodePacket(p);
}
#endif

//// Slot
  
void Slot::flip() {
  if (updatePacket->length() == 0)  return; // no update
  Packet* tmp = activePacket;
  activePacket = updatePacket;
  updatePacket = tmp;
  updatePacket->reset();
}

#ifdef TESTING
void testSlot() {
  Slot s;
  s.update().
    withF1Cmd(998, 0b00010101).
    withThrottleCmd(999, 99, true, false).
    withIdleCmd();
  decodePacket(s); // not flipped yet, should be empty
  s.flip();  
  decodePacket(s); // now it should contain the cmds above
}
#endif

//// Buffer

Buffer::Buffer() {
  // initially, each active packet is IDLE, each update packet is empty
  for (int i=0; i<SLOTS; ++i) {
    slots[i].update().withIdleCmd();
    slots[i].flip();
  }
  // this is a valid bit, because we have all these idle cmds:
  currentSlot = 0;
  currentBit = 0;
}

bool Buffer::nextBit() {
  boolean res = slots[currentSlot].getBit(currentBit);
  currentBit++;
  if (currentBit >= slots[currentSlot].length()) {
    // next slot
    currentSlot = (currentSlot + 1) % SLOTS;
    slots[currentSlot].flip(); // if there is an update, flip into active
    currentBit = 0; // this assneumes that the slot is not empty!
  }  
  return res;
}

#ifdef TESTING

bool Buffer::test() {
  // collect all bits and try to decode
  TestBitStream bs;
  currentBit = 0;
  currentSlot = 0;
  // loop until we are back at the beginning
  slot(1).update().withThrottleCmd(999, 1, true, false);
  slot(9).update().withF1Cmd(999, 0b00010101);
  while (true) {
    int oldCurrentSlot = currentSlot;
    bs.add(nextBit());
    if (oldCurrentSlot != currentSlot) {
      decodePacket(bs);    
      bs.reset();
    }
    if (currentBit == 0 && currentSlot==0) break;
  }
}
#endif
