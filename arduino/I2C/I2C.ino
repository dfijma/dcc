#include <Wire.h>

byte cmd;

void receiveData(int byteCount) {
  Serial.print("read: bc="); Serial.println(byteCount);
  if (byteCount) {
    cmd  = Wire.read();
    Serial.print("cmd: "); Serial.println(cmd);
    byteCount--;
  }

  byte b;
  for (int i=0; i<byteCount; ++i) {
    b = Wire.read();
    Serial.print("b:" ); Serial.println(b);
  }
}

void sendData() {
  Serial.println("write");
  
  for (int i=0; i<1; ++i) {
    Serial.print("send: "); Serial.println(42 + i);
    Wire.write(i);
  }
}

void setup() {
  // put your setup code here, to run once:
  // define callbacks for i2c input
  Wire.onRequest(sendData);
  Wire.onReceive(receiveData);
  Wire.begin(0x04);
  Serial.begin(57600);
  Serial.println("test");

}

void loop() {
}
