#pragma once

#include <Arduino.h>
#include "Config.h"
#include "RefreshBuffer.h"
#include "Current.h"
#include <LocoNet.h>

//// serial communication and command execution

// Notications and responses from controller to host:

// # On setup:
// HELO

// Forward received Loconet package
//   LN A0 11 22 F0
//   - raw packet, including checksum

// Unsollicitated power off (overload)
//   POFF

// ERROR in command
//   ERROR <string>

// Confirmation of command
//   OK <string>

// Commands from host to controller

// set slot:
//   S <slot> <address> <speed> <direction> <function-bits>
//   -example: S 2 126 1 1 0000 0000 0000 
//   -<slot> is decimal slotnumber 0..SLOT-1
//   -<address> is decimal address 1..9999
//   -<speed> is 0..126
//   -<direction> is 1: forward, 0: backwards
//   -<function-bits> is string of 0's and 1's, FL-F4-F3-F2-F1-F8-F7-F6-F5-F12-F11-F10-F9, all optional (padded with 0's if not specified)


// send loconet packet
//   L <byte>
//   L A0 11 22
//   max 15 bytes, no checkum, this is calculated 

// emergency stop
//   N <slot> <address>
//   - write speed = "1" to <slot> <address>

// power on
//   P
//   - switch on track power

// power off
//   O
//   - switch off track power


class Com {
  public:
    Com() {};
    void setup();
    void executeOn(RefreshBuffer& buffer, Current& current);
    void sendLoconet(lnMsg* packet);
    void sendPowerState();
  private:
    byte cmdBuffer[80];
    int cmdLength=0;
};
