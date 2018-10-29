#include "Arduino.h"
#include "Config.h"

#include "I2C.h"
#include <Wire.h>

extern Buffer buffer;

// callback for received data
static void receiveData(int byteCount) {
  // read a single i2c block data transaction
  int count = 0;
  byte bs[32];

  // read the frame and keep max 32 bytes
  while (Wire.available()) {
    byte b  = Wire.read();
    if (count < 32) bs[count] = b;
    count++;
  }
  
  Serial.print("command receive: "); Serial.print(count); Serial.print(": ");
  for (int i = 0; i < count; i++) {
    Serial.print(bs[i]);
    Serial.print(" ");
  }
  Serial.println();
  return;

  if (count < 1) return;

  if (bs[0] == 77) {
    /* update slot
    1 : slot
    2 : address
    3 : speed
    4 : fl
    */
    if (count < 5) return;
    Serial.print("slot: "); Serial.print(bs[0]); 
    Serial.print(" address: "); Serial.print(bs[1]); 
    Serial.print(" speed: "); Serial.print(bs[2]); 
    Serial.print(" direction: "); Serial.print(bs[3]); 
    Serial.print(" fl: " ); Serial.println(bs[4]); 
    buffer.slot(bs[0]).update()
      .withThrottleCmd(bs[1], bs[2], bs[3], false)
      .withF1Cmd(bs[1], bs[4] ? 0b00010000: 0);      
  }
}

// callback for sending data
static void sendData() {
  // Wire.write(ack);
}

void I2C_setup() {
  // initialize i2c as slave
  Wire.begin(SLAVE_ADDRESS);
  // define callbacks for i2c input
  Wire.onReceive(receiveData);
  Wire.onRequest(sendData);
}




