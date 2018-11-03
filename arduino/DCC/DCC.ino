#include "Config.h"
#include <LocoNet.h>
#include "RefreshBuffer.h"
#include "Current.h"
#include "Com.h"

//// Globals

// The current monitor
Current current;

// The refresh buffer
RefreshBuffer buffer;

// com (it means both "command" and "communication" :-)
Com com;

// loconet stuff
lnMsg* loconetPacket;

//// DCC signal modulating

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

//// main setup() and loop()

void setup() {
  com.setup();
  LocoNet.init();
#ifdef TESTING
  testPacket();
  testSlot();
  buffer.slot(0).update().withThrottleCmd(1000, 100, true, 0);
  buffer.test();  
#endif  
  setupDCC();
  // track power is left off, we need an explicit command to turn it on
}

void loop() {
  // cmd execution on new serial data
  com.executeOn(buffer, current);
  
  // receive loconet packages and send through serial port
  loconetPacket = LocoNet.receive() ;
  if (loconetPacket) {
    com.sendLoconet(loconetPacket);
  }

  // check current (the stuff caused by floating electrons)
  if (!current.check()) {
    com.sendPowerState();
  }
}


