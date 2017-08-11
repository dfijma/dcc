#include <Wire.h>

// i2c used for input
#define SLAVE_ADDRESS 0x04

// buffer of DCC data to modulate
// current work, a byte to send and a bitmask pointing to bit being sent
// if currentBitMask is 0, we are idle
// this is still TODO, just a single byte buffer at this time
volatile byte currentByte;
volatile byte currentBitMask;
volatile byte buffered = B10101010;

volatile byte currentByteProg;
volatile byte currentBitMaskProg;
volatile byte bufferedProg = B01010101;

// DCC signal modulating

// basic idea is as follows:
// - we use a timer to generate the DCC signal from the input bits
// - timer interrupt pin outputs the signal
// - timer is programmed to count from 0 to TOP in a period of a full DCC cycle duration (long for a DCC ZERO bit, short for a DCC ONE bit)
// - after a _half_ cycle, interrupt pin goes from LOW to HIGH, and:
// - the interrupt itself is handled to reconfigure the timer for the next cycle based on the value of the next input bit
// - we do this for main track via timer 1 (high resolution timer) and for prog track via timer 0 (low resolution timer)
// - output timer 0 is fed into motor shield channel A to boost main signal to tracks
// - output timer 1 is fed into motor shield channel B to boost signal to programming track

// DCC main signal generated using timer 1 via OC1B interrupt pin (pin 10)
#define DCC_SIGNAL_PIN_MAIN 10
// DCC prog signal generated using timer 0 via OC0B interrupt pin (pin 5)
#define DCC_SIGNAL_PIN_PROG 5

// the DCC signals are, via hardware jumper, fed into direction pin of motor shield, actual direction output pin is to be disabled
// main signal
#define DIRECTION_MOTOR_CHANNEL_PIN_A 12
// prog signal
#define DIRECTION_MOTOR_CHANNEL_PIN_B 13

// timer values for generating DCC ZERO and DCC ONE bits (ZERO cycle is 2 * 100 microseconds, ONE cycle is 2 * 58 microseconds)
// values for 16 bit timer 1 based on 16Mhz clock and a 1:1 prescale
#define DCC_ZERO_BIT_TOTAL_DURATION_TIMER1 3199
#define DCC_ZERO_BIT_PULSE_DURATION_TIMER1 1599

#define DCC_ONE_BIT_TOTAL_DURATION_TIMER1 1855
#define DCC_ONE_BIT_PULSE_DURATION_TIMER1 927

// values for 8 bit timer 0 based on 16Mhz clock and a 1:64 prescale
#define DCC_ZERO_BIT_TOTAL_DURATION_TIMER0 49
#define DCC_ZERO_BIT_PULSE_DURATION_TIMER0 24

#define DCC_ONE_BIT_TOTAL_DURATION_TIMER0 28
#define DCC_ONE_BIT_PULSE_DURATION_TIMER0 14

void setup() {
  Serial.begin(9600);
 
  // initialize i2c as slave
  Wire.begin(SLAVE_ADDRESS);
 
  // define callbacks for i2c input
  Wire.onReceive(receiveData);
  Wire.onRequest(sendData);

  // disable 'real' output pin of motor direction (drive through timer output instead using jumper wire)
  pinMode(DIRECTION_MOTOR_CHANNEL_PIN_A,INPUT);
  digitalWrite(DIRECTION_MOTOR_CHANNEL_PIN_A,LOW);
  pinMode(DCC_SIGNAL_PIN_MAIN, OUTPUT);
  pinMode(DIRECTION_MOTOR_CHANNEL_PIN_B,INPUT);
  digitalWrite(DIRECTION_MOTOR_CHANNEL_PIN_B,LOW);
  pinMode(DCC_SIGNAL_PIN_PROG, OUTPUT);

  // configure timer 1 (for DCC main signal), fast PWM from BOTTOM to OCR1A
  bitSet(TCCR1A,WGM10);     
  bitSet(TCCR1A,WGM11);
  bitSet(TCCR1B,WGM12);
  bitSet(TCCR1B,WGM13);

  // configure timer 1, set OC1B interrupt pin (pin 10) on Compare Match, clear at BOTTOM (inverting mode)
  bitSet(TCCR1A,COM1B1);   
  bitSet(TCCR1A,COM1B0);

  // configure timer1, set prescale to 1
  bitClear(TCCR1B,CS12);    
  bitClear(TCCR1B,CS11);
  bitSet(TCCR1B,CS10);

  // configure timer: 
  // TOP = OCR1A = DCC_ONE_BIT_TOTAL_DURATION_TIMER1
  // toggle OC1B on 'half' time DCC_ONE_BIT_PULSE_DURATION_TIMER1
  OCR1A=DCC_ONE_BIT_TOTAL_DURATION_TIMER1;
  OCR1B=DCC_ONE_BIT_PULSE_DURATION_TIMER1;

  // finally, configure interrupt
  bitSet(TIMSK1,OCIE1B);

  // same idea for timer 0 for prog track 
  // fast PWM
  bitSet(TCCR0A,WGM00);    
  bitSet(TCCR0A,WGM01);
  bitSet(TCCR0B,WGM02);
       
  // set OC0B interrupt pin (pin 5) on Compare Match, clear at BOTTOM (inverting mode)
  bitSet(TCCR0A,COM0B1);
  bitSet(TCCR0A,COM0B0);

  // prescale=64
  bitClear(TCCR0B,CS02);    
  bitSet(TCCR0B,CS01);
  bitSet(TCCR0B,CS00);

  // TOP full cycle, toggle and interrupt at half cycle
  OCR0A=DCC_ONE_BIT_TOTAL_DURATION_TIMER0;
  OCR0B=DCC_ONE_BIT_PULSE_DURATION_TIMER0;

  // enable interrup OC0B
  bitSet(TIMSK0,OCIE0B);

  // init input buffers
  currentByte = buffered;
  currentBitMask = B10000000;

  currentByteProg = bufferedProg;
  currentBitMaskProg = B10000000;
}

ISR(TIMER1_COMPB_vect){         
  // actual DCC output is handled by timer hardware (flipping from LOW to HIGH at half cycle)
  // when handling the interrupt itself, the only way we need to do is reconfigure timer for next cycle
  // we can reconfigure timer for next cycle while current cycle is only half way because the relevant registers
  // are double latched (and only really loaded in the actual registers at next time timer flows over from TOP to BOTTOM
  
  // next bit, please
  if (currentByte & currentBitMask) {
    // now we need to sent "1"
    // set OCR1A for next cycle to full cycle of DCC ONE bit
    // set OCR1B for next cycle to half cycle of DCC ONE bit
    OCR1A=DCC_ONE_BIT_TOTAL_DURATION_TIMER1;                         
    OCR1B=DCC_ONE_BIT_PULSE_DURATION_TIMER1;
  } else {
    // now we need to sent "1"
    // set OCR1A for next cycle to full cycle of DCC ZERO bit
    // set OCR1B for next cycle to half cycle of DCC ZERO bit
    OCR1A=DCC_ZERO_BIT_TOTAL_DURATION_TIMER1;
    OCR1B=DCC_ZERO_BIT_PULSE_DURATION_TIMER1;
  }

  currentBitMask = currentBitMask >> 1;

  // if done, take next byte of work
  // for, now a arbitrary byte if we run out of work
  if (!currentBitMask) {
    // take last sent byte
    currentByte = buffered;
    currentBitMask  = B10000000; // starting with MSB      
  }  
}

// code duplication for handler for prog track is intentional

ISR(TIMER0_COMPB_vect) {         
  // idea is exactly the same
  
  // next bit, please
  if (currentByteProg & currentBitMaskProg) {
    // now we need to sent "1"
    // set OCR1A for next cycle to full cycle of DCC ONE bit
    // set OCR1B for next cycle to half cycle of DCC ONE bit
    OCR0A=DCC_ONE_BIT_TOTAL_DURATION_TIMER0;
    OCR0B=DCC_ONE_BIT_PULSE_DURATION_TIMER0;
  } else {
    // now we need to sent "1"
    // set OCR1A for next cycle to full cycle of DCC ZERO bit
    // set OCR1B for next cycle to half cycle of DCC ZERO bit
    OCR0A=DCC_ZERO_BIT_TOTAL_DURATION_TIMER0;
    OCR0B=DCC_ZERO_BIT_PULSE_DURATION_TIMER0;
  }

  currentBitMaskProg = currentBitMaskProg >> 1;

  // if done, take next byte of work
  // for, now a arbitrary byte if we run out of work
  if (!currentBitMaskProg) {
    // take last sent byte
    currentByteProg = bufferedProg;
    currentBitMaskProg  = B10000000; // starting with MSB      
  }  
}
 
void loop() {
  // main loop is empty
}
 
// callback for received data
void receiveData(int byteCount) {
  // read a single i2c block data transaction
  int count = 0;
  byte buffer[32];
  
  while (Wire.available() && count<32) {
    buffer[count] = Wire.read();
    count++;
  }
  Serial.print(count); Serial.print(": ");
  for (int i=0; i<count; i++) {
    Serial.print(buffer[i]);
    Serial.print(" ");
  }
  Serial.println();
}
 
// callback for sending data
void sendData(){
  Serial.println("asdf");
  Wire.write(1);
  Wire.write(42);
}

