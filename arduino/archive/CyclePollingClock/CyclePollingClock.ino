#include <Wire.h>
 
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
  currentBitMask = B00000000; // so we are actually idle
  started = micros();
  lastChange = started;
  holdUntil = started; // and we don't need to hold current state, go to work as soon as possible
}
 
void loop() {
  unsigned long currentTime = micros();
  unsigned long actualHalfCycle;
  unsigned long error;
  
  //if (currentTime > started + 5000000) { 
  //  Serial.println(halfCycleError / halfCycleCount);
  //  return; 
  //}

  // do something after waited long enough
  if (currentTime >= holdUntil) {
    switch (state) {
      case STATE_HIGH:
        // goto state low for next half-cycle
        digitalWrite(SIGNAL, LOW);
        currentTime = micros();
        actualHalfCycle = currentTime - lastChange;
        error = abs(actualHalfCycle - halfCycle);
        // Serial.print("we were HIGH for "); Serial.println(actualHalfCycle);
        lastChange = currentTime;
        holdUntil = lastChange + halfCycle;
        state = STATE_LOW;
        // and wait for next time
        return;
      case STATE_LOW:
        // cycle completed
        // shift for next bit in current Byte
        currentBitMask = currentBitMask >> 1;
        break;        
    }

    // if done, take next byte of work
    // for, now a arbitrary byte if we run out of work
    if (!currentBitMask) {
      // take last sent byte
      currentByte = buffered;
      // Serial.print("next byte "); Serial.println(currentByte);
      currentBitMask  = B10000000; // starting with MSB      
    }

    digitalWrite(SIGNAL, HIGH);
    currentTime = micros();
    actualHalfCycle = currentTime - lastChange;
    error = abs(actualHalfCycle - halfCycle);
    // Serial.print("we were LOW for "); Serial.println(actualHalfCycle);
    // halfCycle = (currentByte & currentBitMask) ? 6 : 3; // 500 ms for a one, 250 ms for a zero
    if (currentByte & currentBitMask) {
      // Serial.println("sending 1");
      halfCycle = 100;
    } else {
      // Serial.println("sending 0");
      halfCycle = 58;
    }
    lastChange = currentTime;
    holdUntil = lastChange + halfCycle;
    state = STATE_HIGH;
  }
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

