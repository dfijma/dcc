#include "Config.h"
#include "Buffer.h"
#include "Current.h"
#include "I2C.h"
#include <LocoNet.h>

// The current monitor
Current current;

// The refresh buffer
Buffer buffer;

// DCC signal modulating

// basic idea is as follows:
// - we use a timer to generate the DCC signal from the input bits
// - timer interrupt pin outputs the signal
// - timer is programmed to count from 0 to TOP in a period of a full DCC cycle duration (long for a DCC ZERO bit, short for a DCC ONE bit)
// - after a _half_ cycle, interrupt pin goes from LOW to HIGH, and:
// - the interrupt itself is handled to reconfigure the timer for the next cycle based on the value of the next input bit
// - we do this for main track via timer 0 (low resolution timer) and for prog track via timer 2 (low resolution timer)
// - original code uses timer1, but that is needed for the LocaNet shield
// - output timer 0 is fed into motor shield channel A to boost main signal to tracks
// - output timer 2 is fed into motor shield channel B to boost signal to programming track

// DCC main signal generated using timer 0 via OC0B interrupt pin (pin 5)
#define DCC_SIGNAL_PIN_MAIN 5
// DCC prog signal generated using timer 2 via OC2B interrupt pin (pin 3)
#define DCC_SIGNAL_PIN_PROG 3 // TODO: this actually conflicts with SPEED_MOTOR_SHIELD_PIN_A

// the DCC signals are, via hardware jumper, fed into direction pin of motor shield, actual direction output pin is to be disabled
// main signal
#define DIRECTION_MOTOR_CHANNEL_PIN_A 12
#define SPEED_MOTOR_CHANNEL_PIN_A 3
#define BRAKE_MOTOR_CHANNEL_PIN_A 9
// prog signal
#define DIRECTION_MOTOR_CHANNEL_PIN_B 13
#define SPEED_MOTOR_CHANNEL_PIN_B 11
#define BRAKE_MOTOR_CHANNEL_PIN_B 8

// timer values for generating DCC ZERO and DCC ONE bits (ZERO cycle is 2 * 100 nanoseconds, ONE cycle is 2 * 58 microseconds)
// values for 16 bit timer 1 based on 16Mhz clock and a 1:1 prescale
#define DCC_ZERO_BIT_TOTAL_DURATION_TIMER1 3199
#define DCC_ZERO_BIT_PULSE_DURATION_TIMER1 1599

#define DCC_ONE_BIT_TOTAL_DURATION_TIMER1 1855
#define DCC_ONE_BIT_PULSE_DURATION_TIMER1 927

// so: 0 bit half cycle =  1599 * 1 ticks / 16 Mhz = 100 nanoseconds
//     1 bit half cysle = 927 * 1 ticks / 16 Mhz = 58 nanoseconds 

// values for 8 bit timer 0 (and 2??) based on 16Mhz clock and a 1:64 prescale
#define DCC_ZERO_BIT_TOTAL_DURATION_TIMER0 49 // exact: 50
#define DCC_ZERO_BIT_PULSE_DURATION_TIMER0 24 // exact: 25

#define DCC_ONE_BIT_TOTAL_DURATION_TIMER0 28 // exact: 29
#define DCC_ONE_BIT_PULSE_DURATION_TIMER0 14 // exact: 14,5

// so: 0 bit half cycle = 24 * 64 ticks / 16Mhz = 96 nanoseconds
//     1 bit half cyclse = 14 * 64 ticks / 16Mhz = 56 nanoseconds

void setupDCC() {

  pinMode(DCC_SIGNAL_PIN_MAIN, OUTPUT);
  pinMode(DCC_SIGNAL_PIN_PROG, OUTPUT);

  // configure timer 0 (for DCC main signal), fast PWM from BOTTOM to TOP == OCR0A
  // fast PWM
  bitSet(TCCR0A, WGM00);
  bitSet(TCCR0A, WGM01);
  bitSet(TCCR0B, WGM02);

  // set OC0B interrupt pin (pin 5) on Compare Match == OCR0B, clear at BOTTOM (inverting mode)
  bitSet(TCCR0A, COM0B1);
  bitSet(TCCR0A, COM0B0);

  // prescale=64
  bitClear(TCCR0B, CS02);
  bitSet(TCCR0B, CS01);
  bitSet(TCCR0B, CS00);

  // TOP full cycle, toggle and interrupt at half cycle
  OCR0A = DCC_ONE_BIT_TOTAL_DURATION_TIMER0;
  OCR0B = DCC_ONE_BIT_PULSE_DURATION_TIMER0;

  // enable interrup OC0B
  bitSet(TIMSK0, OCIE0B);

  /* TODO: enable prog track once conflict in pin3 solved
  // TOP full cycle, toggle and interrupt at half cycle
  OCR2A = DCC_ONE_BIT_TOTAL_DURATION_TIMER0;
  OCR2B = DCC_ONE_BIT_PULSE_DURATION_TIMER0;
  
  // configure time 2 (for DCC prog signal), fast OWM from BOTTOM to TOP == OCR2A
  bitSet(TCCR2A, WGM20);
  bitSet(TCCR2A, WGM21);
  bitSet(TCCR2B, WGM22);

  // set OC2B interrupt (pin 3??) on Compare Match = 0CR2B, clear at Bottom (inverting mode)
  bitSet(TCCR2A, COM2B1);
  bitSet(TCCR2A, COM2B0);

  // prescale = 64
  bitSet(TCCR2B, CS22);
  bitClear(TCCR2B, CS01);
  bitClear(TCCR2B, CS00);
  
  // enable interrupt OC2B
  bitSet(TIMSK2, OCIE2B);
  */
}

ISR(TIMER0_COMPB_vect) {
  // actual DCC output is handled by timer hardware (flipping from LOW to HIGH at half cycle)
  // when handling the interrupt itself, the only way we need to do is reconfigure timer for next cycle
  // we can reconfigure timer for next cycle while current cycle is only half way because the relevant registers
  // are double latched (and only really loaded in the actual registers at next time timer flows over from TOP to BOTTOM
  // next bit, please
  if (buffer.nextBit()) {
    // now we need to sent "1"
    // set OCR0A for next cycle to full cycle of DCC ONE bit
    // set OCR0B for next cycle to half cycle of DCC ONE bit
    OCR0A = DCC_ONE_BIT_TOTAL_DURATION_TIMER0;
    OCR0B = DCC_ONE_BIT_PULSE_DURATION_TIMER0;
  } else {
    // now we need to sent "0"
    // set OCR0A for next cycle to full cycle of DCC ZERO bit
    // set OCR0B for next cycle to half cycle of DCC ZERO bit
    OCR0A = DCC_ZERO_BIT_TOTAL_DURATION_TIMER0;
    OCR0B = DCC_ZERO_BIT_PULSE_DURATION_TIMER0;
  }
}

// code duplication for handler for prog track is intentional

/*
ISR(TIMER2_COMPB_vect) {
    // prog track idem
    // next bit, please
  if (nextBit(&progBuffer)) {
    // now we need to sent "1"
    // set OCR2A for next cycle to full cycle of DCC ONE bit
    // set OCR2B for next cycle to half cycle of DCC ONE bit
    OCR2A = DCC_ONE_BIT_TOTAL_DURATION_TIMER0;
    OCR2B = DCC_ONE_BIT_PULSE_DURATION_TIMER0;
  } else {
    // now we need to sent "0"
    // set OCR2A for next cycle to full cycle of DCC ZERO bit
    // set OCR2B for next cycle to half cycle of DCC ZERO bit
    OCR2A = DCC_ZERO_BIT_TOTAL_DURATION_TIMER0;
    OCR2B = DCC_ZERO_BIT_PULSE_DURATION_TIMER0;
  }
}
*/

long last = 0;

/*
void update(bool direction) {
  buffer.slot(0).update().withThrottleCmd(3, 0, direction, false);
  buffer.slot(0).update().withF1Cmd(3, 0b00011111);
  buffer.slot(0).flip();
  buffer.slot(1).update().withThrottleCmd(4, 0, direction, false);
  buffer.slot(1).update().withF1Cmd(4, 0b00011111);
  buffer.slot(1).flip();
} 
*/

// cmd execution

void setSlot(int slot, int address, int speed, int fns) {
  if (slot >= SLOTS) {
    Serial.println("ERROR:slot max");
    return;
  }
  if (address >= 9999) {
    Serial.println("ERROR:address max");
    return;
  }
  if (speed > 255) {
    Serial.println("ERROR:speed invalid");
    return;
  }
  Serial.print("OK SLOT="); Serial.print(slot); Serial.print(",ADR="); Serial.print(address); Serial.print(",SPD=" ); Serial.print(speed); Serial.print(",FN="); Serial.print("0b"); Serial.print(fns, 2); Serial.println() ;   
}

void sendLoconet(byte bs[], int len) {
   Serial.print("OK:LOCONET="); for (int i=0; i<len; ++i) { Serial.print(bs[i]); Serial.print(" "); } Serial.println(); 
}

// cmd parsing
byte cmdBuffer[80];
int cmdLength=0;

boolean parseHexDigit(int& res, byte*& cmd) {
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

boolean parseHexByte(int &res, byte*& cmd) {
  int b1;
  if (!parseHexDigit(b1, cmd)) return false;
  if (!parseHexDigit(res, cmd)) return false;
  res = b1*16 + res; return true;
}

boolean parseDigit(int& res, byte* &cmd, byte m='9') {
  byte b = *cmd;
  if (b>='0' && b<=m) {
    cmd++;
    res = b - '0';
    return true;
  }
  return false;
}

boolean skipWhiteSpace(byte*& cmd) {
  while ((*cmd) == ' ') cmd++;
}

boolean parseNumber(int& res, byte*& cmd) {
  if (!parseDigit(res, cmd)) return false;
  int next;
  while (parseDigit(next, cmd)) { res = res*10 + next; }
  return true;
}

void parse(byte *cmd) {
  if (*cmd == 'S') {
    cmd++;
    int slot;
    int address;
    int speed;
    
    skipWhiteSpace(cmd);
    if (!parseNumber(slot, cmd)) {
      Serial.println("ERROR:no slot");
      return;
    }

    skipWhiteSpace(cmd);
    if (!parseNumber(address, cmd)) {
      Serial.println("ERROR:no address");
      return;
    }

    skipWhiteSpace(cmd);
    if (!parseNumber(speed, cmd)) {
      Serial.println("ERROR:no speed");
      return;
    }

    skipWhiteSpace(cmd);
    int fn;
    int fns = 0;
    int ifns = 0;
    skipWhiteSpace(cmd);
    while (ifns < 13 && parseDigit(fn, cmd, '1')) { skipWhiteSpace(cmd); fns = (fns<<1) | fn; ifns++; }
    fns = fns << (13-ifns);
    if (*cmd != 0) {
      Serial.println("ERROR:extra chars");
      return;
    }
    setSlot(slot, address, speed, fns);
  } else if (*cmd == 'L') {
    cmd++;
    byte buffer[127];
    int len=0;
    skipWhiteSpace(cmd);
    int lb;
    while (len < 127 && parseHexByte(lb, cmd)) { skipWhiteSpace(cmd); buffer[len++] = lb; };
    if (*cmd != 0) {
      Serial.println("ERROR:extra chars");
      return;
    }
    sendLoconet(buffer, len);
  } else {
    Serial.println("ERROR: no cmd");
  }
}

// loconet stuff
lnMsg *LnPacket;

void setup() {
  Serial.begin(57600);
  LocoNet.init();
#ifdef TESTING
  // testPacket();
  // testSlot();
  // buffer.slot(0).update().withThrottleCmd(1000, 100, true, 0);
  // buffer.test();  
#endif  
  setupDCC();
  I2C_setup();
  current.on();
  Serial.println("H");
}

void loop() {

  // parse and execute received serial data
  while (Serial.available()) {
    byte b = Serial.read();
    if (b == '\r') continue; // bye windows
    if (b == '\n') {
      // EOL, parse it
      cmdBuffer[cmdLength++] = '\0';
      parse(cmdBuffer);
      cmdLength = 0;
      continue;
    }

    // buffer byte, if room for it
    if (cmdLength < (sizeof(cmdBuffer)-1)) {
      cmdBuffer[cmdLength] = b;
      cmdLength++;
    }   
  }

  // receive loconet packages and send through serial port
  LnPacket = LocoNet.receive() ;
  if (LnPacket) {
    uint8_t msgLen = getLnMsgSize(LnPacket);
    Serial.print("L ");
    for (uint8_t x = 0; x < msgLen; x++) {
      uint8_t val = LnPacket->data[x];
      Serial.print(val, 16); Serial.print(" "); 
    }
    Serial.println();
  }
  
  current.check(); // check current   
}


