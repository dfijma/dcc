#include <Wire.h>

// i2c used for input
#define SLAVE_ADDRESS 0x04


void setup() {
  // serial
  Serial.begin(57600);

  // initialize i2c as slave
  Wire.begin(SLAVE_ADDRESS);

  // define callbacks for i2c input
  Wire.onReceive(receiveData);
  Wire.onRequest(sendData);
}

void loop() {
}

// ack data from receive to transmit
volatile byte ack;

// callback for received data
void receiveData(int byteCount) {
  // read a single i2c block data transaction
  int count = 0;
  byte buffer[32];

  // read the frame and keep max 32 bytes
  while (Wire.available()) {
    byte b  = Wire.read();
    if (count < 32) buffer[count] = b;
    count++;
  }
  Serial.print("receive: "); Serial.print(count); Serial.print(": ");
  for (int i = 0; i < count; i++) {
    Serial.print(buffer[i]);
    Serial.print(" ");
  }
  Serial.println();

  ack = 1; // success
}

// callback for sending data
void sendData() {
  Serial.print("send: ");
  int len = 2;
  for (int i=0; i<len; ++i) {
    Serial.print(i); Serial.print(" ");
    Wire.write(i+1);
  } 
  Serial.println();
}


