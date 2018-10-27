#include <LocoNet.h>

// forward loconet packages via serial

lnMsg        *LnPacket;

void setup() {
  LocoNet.init();
  Serial.begin(57600);
}

bool sent = false;

void loop() {
  // receive loconet packages and send through serial port
  LnPacket = LocoNet.receive() ;
  if (LnPacket) {
    uint8_t msgLen = getLnMsgSize(LnPacket);
    for (uint8_t x = 0; x < msgLen; x++) {
      uint8_t val = LnPacket->data[x];
      Serial.write(val);
    }
  }
  // LocoNet.send(0xBF, 1, 22);
}
