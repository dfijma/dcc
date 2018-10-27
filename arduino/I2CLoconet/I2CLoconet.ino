#include <Wire.h>
#include <LocoNet.h>

// loconet monitor

lnMsg        *LnPacket;


class I2CBuffer {
  public:
    I2CBuffer() {
      reset();
    }
    byte length() { return end-next; }
    byte take() { return packet[next++]; }
    void put(byte b) { packet[end++] = b; }
    void reset() { next = end = 0; }
  private:
    byte packet[32];
    byte next;
    byte end;
};

volatile I2CBuffer buffers[2];
I2CBuffer *currentBuffer = &buffers[0];
I2CBuffer *updateBuffer = &buffers[1];

int count = 0;

void receiveData(int byteCount) {
  byte cmd  = Wire.read();

  byte b;
  while (Wire.available()) {
    count++;
    b = Wire.read();
  }
}

void sendData() {
  // send remainder of currentBuffer or take next updated buffer
  if (currentBuffer->length() == 0) {
    // currentBuffer empty, swap for (possible) next updateBuffer
    I2CBuffer *tmp = currentBuffer;
    currentBuffer = updateBuffer;
    updateBuffer = tmp;
  }
  // currentBuffer can still be empty
  byte l = currentBuffer->length();
  if (l > 31) {
    l = 31; // max i2c transaction excl length
  }
  Wire.write(l);
  for (int i=0; i < l; ++i) {
    Wire.write(currentBuffer->take());
  }
}

void setup() {
  // put your setup code here, to run once:
  Wire.begin(0x04);
  // define callbacks for i2c input
  Wire.onReceive(receiveData);
  Wire.onRequest(sendData);

  LocoNet.init();

  Serial.begin(57600);

}

void loop() {

  // receive loconet packages and buffer for i2c transfer
  LnPacket = LocoNet.receive() ;
  if (LnPacket) {
    for(;;) {
      noInterrupts();
      bool ok = updateBuffer->length() == 0;
      if (ok) break;
      interrupts();
      delay(1);
    }

    updateBuffer->reset();
    uint8_t msgLen = getLnMsgSize(LnPacket);

    if (msgLen > 31) {
      // TODO: handle loconet messages that does not fit in a single i2c payload
      return;
    }
    // copy package to i2c buffer
    for (uint8_t x = 0; x < msgLen; x++) {
      updateBuffer->put(LnPacket->data[x]);
    }
    interrupts();
  }
}
