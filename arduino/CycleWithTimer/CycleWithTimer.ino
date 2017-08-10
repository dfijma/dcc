#include <Wire.h>

// i2c used for input
#define SLAVE_ADDRESS 0x04

// buffer of DCC data to modulate
// current work, a byte to send and a bitmask pointing to bit being sent
// if currentBitMask is 0, we are idle
volatile byte currentByte;
volatile byte currentBitMask;
volatile byte buffered = B10101010;

volatile byte currentByteProg;
volatile byte currentBitMaskProg;
volatile byte bufferedProg = B10101010;

// DCC signal modulating

// DCC main signal generated using timer 1 via OC1B interrupt pin (pin 10)
#define DCC_SIGNAL_PIN_MAIN 10
// DCC prog signal generated using timer 0 via OC0B interrupt pin (pin 5)
#define DCC_SIGNAL_PIN_PROG 5

// the DCC signals are, via hardware jumper, fed into direction pin of motor shield, actual direction output pin is to be disabled
// main signa, 
#define DIRECTION_MOTOR_CHANNEL_PIN_A 12
#define DIRECTION_MOTOR_CHANNEL_PIN_B 13

// timer values for generating DCC ZERO and DCC ONE bits
#define DCC_ZERO_BIT_TOTAL_DURATION_TIMER1 3199
#define DCC_ZERO_BIT_PULSE_DURATION_TIMER1 1599

#define DCC_ONE_BIT_TOTAL_DURATION_TIMER1 1855
#define DCC_ONE_BIT_PULSE_DURATION_TIMER1 927

#define DCC_ZERO_BIT_TOTAL_DURATION_TIMER0 49
#define DCC_ZERO_BIT_PULSE_DURATION_TIMER0 24

#define DCC_ONE_BIT_TOTAL_DURATION_TIMER0 28
#define DCC_ONE_BIT_PULSE_DURATION_TIMER0 14

void setup() {
  Serial.begin(9600);
 
  // initialize i2c as slave
  Wire.begin(SLAVE_ADDRESS);
 
  // define callbacks for i2c communication
  Wire.onReceive(receiveData);
  Wire.onRequest(sendData);

  // disable 'real' output pin of motor direction (drive through timer output instead using jumper wire)
  pinMode(DIRECTION_MOTOR_CHANNEL_PIN_A,INPUT);
  digitalWrite(DIRECTION_MOTOR_CHANNEL_PIN_A,LOW);
  pinMode(DCC_SIGNAL_PIN_MAIN, OUTPUT);
  pinMode(DIRECTION_MOTOR_CHANNEL_PIN_B,INPUT);
  digitalWrite(DIRECTION_MOTOR_CHANNEL_PIN_B,LOW);
  pinMode(DCC_SIGNAL_PIN_PROG, OUTPUT);

  // configure timer 1 (for main DCC signal, to channel A of motor shield)
  // fast PWM from BOTTOM to OCR1A
  bitSet(TCCR1A,WGM10);     
  bitSet(TCCR1A,WGM11);
  bitSet(TCCR1B,WGM12);
  bitSet(TCCR1B,WGM13);

  // configure timer:
  // Set OC1B interrupt pin (pin 10) on Compare Match, clear OC1B at BOTTOM (inverting mode)
  bitSet(TCCR1A,COM1B1);   
  bitSet(TCCR1A,COM1B0);

  // configure timer:
  // set Timer 1 prescale=1
  bitClear(TCCR1B,CS12);    
  bitClear(TCCR1B,CS11);
  bitSet(TCCR1B,CS10);

  // configure timer: 
  // TOP = OCR1A = DCC_ONE_BIT_TOTAL_DURATION_TIMER1
  // toggle OC1B on 'half' time DCC_ONE_BIT_PULSE_DURATION_TIMER1
  OCR1A=DCC_ONE_BIT_TOTAL_DURATION_TIMER1;
  OCR1B=DCC_ONE_BIT_PULSE_DURATION_TIMER1;

  // finally, configure interrupt
  bitSet(TIMSK1,OCIE1B);    // enable interrupt vector for Timer 1 Output Compare B Match (OCR1B)  

  // same idea for timer 0 for prog track (but timer is slightly different, also different resolution)
  // fast PWM
  bitSet(TCCR0A,WGM00);    
  bitSet(TCCR0A,WGM01);
  bitSet(TCCR0B,WGM02);

       
  // Set OC0B interrupt pin (pin 5) on Compare Match, clear OC0B at BOTTOM (inverting mode)
  bitSet(TCCR0A,COM0B1);
  bitSet(TCCR0A,COM0B0);

  // set Timer 0 prescale=64
  bitClear(TCCR0B,CS02);    
  bitSet(TCCR0B,CS01);
  bitSet(TCCR0B,CS00);

  OCR0A=DCC_ONE_BIT_TOTAL_DURATION_TIMER0;
  OCR0B=DCC_ONE_BIT_PULSE_DURATION_TIMER0;

  // enable interrup OC0B
  bitSet(TIMSK0,OCIE0B);
  
  currentByte = 170;
  currentBitMask = B10000000;
}

ISR(TIMER1_COMPB_vect){         
  // set interrupt service for OCR1B of TIMER-1 which flips direction bit of Motor Shield Channel A controlling Main Track
  // reconfigure timer for next bit (double latched so no disturbing current cycle)

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

  // if done, take next byte of work
  // for, now a arbitrary byte if we run out of work
  if (!currentBitMask) {
    // take last sent byte
    currentByte = buffered;
    // Serial.print("next byte "); Serial.println(currentByte);
    currentBitMask  = B10000000; // starting with MSB      
  }  
}

// code duplication intentional 

ISR(TIMER0_COMPB_vect){         
  // set interrupt service for OCR0B of TIMER-0 which flips direction bit of Motor Shield Channel B controlling Prog Track
  // reconfigure timer for next bit (double latched so no disturbing current cycle)

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

  // if done, take next byte of work
  // for, now a arbitrary byte if we run out of work
  if (!currentBitMaskProg) {
    // take last sent byte
    currentByteProg = bufferedProg;
    // Serial.print("next byte "); Serial.println(currentByte);
    currentBitMaskProg  = B10000000; // starting with MSB      
  }  
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

