#include <Wire.h>
#include "TimerOne.h"
 
#define SLAVE_ADDRESS 0x04
#define SIGNAL 13

#define STATE_HIGH 1
#define STATE_LOW 2

int state; // in HIGH cycle, or in LOW cycle

unsigned long started; // for debugging only, to run for a few seconds only

unsigned long holdUntil;
unsigned long lastChange;
unsigned long halfCycle;

// current work, a byte to send and a bitmask pointing to bit being sent
// if currentBitMask is 0, we are idle
byte currentByte;
byte currentBitMask;

byte buffered = B10101010;

unsigned long count;

unsigned long previous;

void timerExpired() {
  unsigned long now = micros();
  Serial.print("trigger "); Serial.println(now - previous);
  previous = now;
  if (state == LOW) {

    // next bit, please
    if (currentByte & currentBitMask) {
      // now we need to sent "1"
      halfCycle = 1000000;
    } else {
      // now we need to sent "0";
      halfCycle = 500000;
    }
    
    currentBitMask = currentBitMask >> 1;

    Timer1.setPeriod(halfCycle);
    Timer1.restart();
    state = HIGH;
    digitalWrite(SIGNAL, HIGH);
  } else {
    state = LOW;
    digitalWrite(SIGNAL, LOW);
  }
}
 
void setup() {

  Serial.begin(9600);

  
  pinMode(SIGNAL, OUTPUT);
 
  // initialize i2c as slave
  Wire.begin(SLAVE_ADDRESS);
 
  // define callbacks for i2c communication
  Wire.onReceive(receiveData);
  Wire.onRequest(sendData);


  

  state = STATE_LOW;
  digitalWrite(SIGNAL, LOW);
  currentByte = 170;
  currentBitMask = B10000000;
  started = micros();
  lastChange = started;
  holdUntil = started; // and we don't need to hold current state, go to work as soon as possible

  Timer1.initialize(1000);
  Timer1.attachInterrupt(timerExpired);


}
 
void loop() {
  unsigned long currentTime = micros();
 
  unsigned long error;
  
  
}
 
// callback for received data
void receiveData(int byteCount){
  while (Wire.available()) {
    buffered = Wire.read();
  }
}
 
// callback for sending data
void sendData(){
 Wire.write(0);
}

