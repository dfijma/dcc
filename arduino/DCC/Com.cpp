#include "Com.h"
#include <LocoNet.h>

// cmd parsing and execution

static boolean parseHexDigit(int& res, byte*& cmd) {
  byte b = *cmd;
  if (b>='0' && b<='9') {
    res = b-'0';
    cmd++;
    return true;
  } else if (b>='a' && b<='f') {
    res = b-'a' + 10;
    cmd++;
    return true;
  } else if (b>='A' && b<='F') {
    res = b-'A'+10;
    cmd++;
    return true;
  }
  return false;
}

static boolean parseHexByte(int &res, byte*& cmd) {
  int b1;
  if (!parseHexDigit(b1, cmd)) return false;
  if (!parseHexDigit(res, cmd)) return false;
  res = b1*16 + res; return true;
}

static boolean parseDigit(int& res, byte* &cmd, byte m='9') {
  byte b = *cmd;
  if (b>='0' && b<=m) {
    cmd++;
    res = b - '0';
    return true;
  }
  return false;
}

static boolean skipWhiteSpace(byte*& cmd) {
  while ((*cmd) == ' ') cmd++;
}

static boolean parseNumber(int& res, byte*& cmd) {
  if (!parseDigit(res, cmd)) return false;
  int next;
  while (parseDigit(next, cmd)) { res = res*10 + next; }
  return true;
}

static boolean parseAndCheckAddress(int& address, byte*& cmd) {
  if (!parseNumber(address, cmd)) {
    Serial.println("ERROR no address");
    return false;
  }
  if (address < MIN_DCC_ADDRESS || address > MAX_DCC_ADDRESS) {
    Serial.println("ERROR invalid address");
    return false;
  }
  return true;
}

static boolean parseAndCheckSlot(int& slot, byte*& cmd) {
  if (!parseNumber(slot, cmd)) {
    Serial.println("ERROR no slot");
    return false;
  }
  if (slot >= SLOTS) {
    Serial.println("ERROR invalid slot");
    return false;
  }
  return true;
}

static void parse(RefreshBuffer& buffer, Current current, byte *cmd) {
  switch (*cmd) {
  case 'S': case 's': {
    // S 12 999 126 1 1 1111111111111 
    // S <SLOT> <ADR> <SPD> <DIR> <F0><F1>..<F12>

    cmd++;
    int slot;
    int address;
    int speed;
    boolean direction;
    int fns;
    int tmp;
    
    skipWhiteSpace(cmd);
    if (!parseAndCheckSlot(slot, cmd)) return;
    skipWhiteSpace(cmd);
    if (!parseAndCheckAddress(address, cmd)) return;
    skipWhiteSpace(cmd);
    
    if (!parseNumber(speed, cmd)) {
      Serial.println("E no speed");
      return;
    }
    if (speed > MAX_DCC_SPEED) {
      Serial.println("ERROR invalid speed");
      return;
    }

    skipWhiteSpace(cmd);
    if (!parseDigit(tmp, cmd, '1')) {
      Serial.println("E no direction");
      return;
    }
    direction = tmp == 1;
    
    int ifns = 0; // bit pattern for functions
    skipWhiteSpace(cmd);
    while (ifns < 13 && parseDigit(tmp, cmd, '1')) { skipWhiteSpace(cmd); fns = (fns<<1) | tmp; ifns++; }
    // all 13 function bits are optional, shift in remaining 0's
    fns = fns << (13-ifns);
    
    // function bits are presented in an order that is easiliy translated to DCC packets
    // FL-F4-F3-F2-F1-F8-F7-F6-F5-F12-F11-F10-F9
    int f1;
    int f2lo;
    int f2hi;
    // FL-F4-F3-F1-F1 goes in f1
    f1   = (fns & 0b1111100000000) >> 8;
    // F8-F7-F6-F5 goes in f2 low
    f2lo = (fns & 0b11110000) >> 4;
    // F12-F11-F10-F9 go in f2 high
    f2hi = (fns & 0b1111);
    
    buffer.slot(slot).update().withThrottleCmd(address, (byte)speed, direction, false).
      withF1Cmd(address, f1).
      withF2LowCmd(address, f2lo).
      withF2HighCmd(address, f2hi);
    Serial.print("OK slot="); Serial.print(slot); Serial.print(",adr="); Serial.print(address); 
      Serial.print(",spd=" ); Serial.print(speed); Serial.print(",dir="); Serial.print(direction); 
      Serial.print(",fns="); Serial.print("0b"); Serial.print(fns, 2); Serial.println() ;   
    }
    break;
  case 'L': case 'l': {
    cmd++;
    lnMsg msg;
    int len=0;
    skipWhiteSpace(cmd);
    int lb;
    // read no more than MAX_LOCONET_PACKET-1 bytes, safe one for checksum
    while (len < MAX_LOCONET_PACKET-1 && parseHexByte(lb, cmd)) { 
      skipWhiteSpace(cmd); 
      msg.data[len++] = lb;
    };

    // we don't check validity of packet, and we do not include checksum (library will handle that)
    LocoNet.send(&msg);
    Serial.print("OK #bytes="); Serial.print(len); Serial.println(); 
    }
    break;
  case 'N': case 'n': {
    // Notfall!
    int slot;
    int address;

    skipWhiteSpace(cmd);
    if (!parseAndCheckSlot(slot, cmd)) return;
    skipWhiteSpace(cmd);
    if (!parseAndCheckAddress(address, cmd)) return;
    
    buffer.slot(slot).update().withThrottleCmd(address, 0, 1, 1);
    Serial.print("OK slot="); Serial.print(slot); Serial.print(",adr="); Serial.print(address); Serial.println(",emergency");
    }
    break;
  case 'P': case 'p': {
    current.on();
    Serial.println("OK power is on");
    break;
  }
  case 'O': case 'o': {
    current.off();
    Serial.println("OK power is off");
    break;
  }
 
  default:
    Serial.println("ERROR no command");
  }
}

void Com::setup() {
  Serial.begin(57600);
  Serial.println("HELO");
}

void Com::executeOn(RefreshBuffer& buffer, Current& current) {
  // parse and execute received serial data
  while (Serial.available()) {
    byte b = Serial.read();
    if (b == '\r') continue; // bye windows
    if (b == '\n') {
      // EOL, parse it
      cmdBuffer[cmdLength++] = '\0';
      parse(buffer, current, cmdBuffer);
      cmdLength = 0;
      continue;
    }

    // buffer byte, if there is room for it (safe one for terminating zero)
    if (cmdLength < (sizeof(cmdBuffer)-1)) {
      cmdBuffer[cmdLength] = b;
      cmdLength++;
    }   
  }
}

void Com::sendLoconet(lnMsg* packet) {
  uint8_t msgLen = getLnMsgSize(packet);
  Serial.print("LN");
  for (uint8_t x = 0; x < msgLen; x++) {
    uint8_t val = packet->data[x];
    Serial.print(" "); Serial.print(val, 16); 
  }
  Serial.println();
}

void Com::sendPowerState() {
  Serial.println("POFF");
}

