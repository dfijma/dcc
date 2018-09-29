#include "Config.h"
#include "Buffer.h"
#include "Current.h"

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

void setup() {
  Serial.begin(57600);
#ifdef TESTING
  // testPacket();
  // testSlot();
  // buffer.slot(0).update().withThrottleCmd(1000, 100, true, 0);
  // buffer.test();  
#endif  
  buffer.slot(0).update().withThrottleCmd(3, 0, true, false);
  buffer.slot(0).update().withF1Cmd(3, 0b00011111);
  buffer.slot(0).flip();
  setupDCC();
  current.on();
}

void loop() {
  current.check(); // check current 
}
