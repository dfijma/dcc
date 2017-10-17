#include <Wire.h>

// i2c used for input
#define SLAVE_ADDRESS 0x04

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
#define SPEED_MOTOR_CHANNEL_PIN_A 3
#define BRAKE_MOTOR_CHANNEL_PIN_A 9
// prog signal
#define DIRECTION_MOTOR_CHANNEL_PIN_B 13
#define SPEED_MOTOR_CHANNEL_PIN_B 11
#define BRAKE_MOTOR_CHANNEL_PIN_B 8

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

  // initialize refresh buffer (with an idle packet)
  initRefreshBuffer();
 
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

  // power on
  pinMode(SPEED_MOTOR_CHANNEL_PIN_A,OUTPUT);   // main track power
  digitalWrite(SPEED_MOTOR_CHANNEL_PIN_A, HIGH);
  pinMode(BRAKE_MOTOR_CHANNEL_PIN_A,OUTPUT);  // release brake
  digitalWrite(BRAKE_MOTOR_CHANNEL_PIN_A, LOW); 
  pinMode(SPEED_MOTOR_CHANNEL_PIN_B,OUTPUT);   // prog track power
  digitalWrite(SPEED_MOTOR_CHANNEL_PIN_B, HIGH);
  pinMode(BRAKE_MOTOR_CHANNEL_PIN_B,OUTPUT);  // release brake
  digitalWrite(BRAKE_MOTOR_CHANNEL_PIN_B, LOW); 
}

ISR(TIMER1_COMPB_vect){         
  // actual DCC output is handled by timer hardware (flipping from LOW to HIGH at half cycle)
  // when handling the interrupt itself, the only way we need to do is reconfigure timer for next cycle
  // we can reconfigure timer for next cycle while current cycle is only half way because the relevant registers
  // are double latched (and only really loaded in the actual registers at next time timer flows over from TOP to BOTTOM
  
  // next bit, please
  if (nextBit()) {
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
}

// code duplication for handler for prog track is intentional

ISR(TIMER0_COMPB_vect) {         
  // idea is exactly the same
  
  // next bit, please
  if (true /* todo: a prog track buffer */) {
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
}
 
void loop() {
  // check current 
  checkCurrent();
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
  for (int i=0; i<count; i++) {
    Serial.print(buffer[i]);
    Serial.print(" ");
  }
  Serial.println();

  if (count < 3) return; // no enough data
  // Just a basic interface to start:
  // cmd: slot
  // data[0]: direction value: 0..126 or 
  // data[1]: direction (0 is backwards, != 0 is forward

  // create a DCC update throttle command
  byte slot = buffer[0];
  byte b[5]; // max 5 bytes, including checksum byte
  int cab = 3; // TODO: for now, hard coded loc 3
  int tSpeed =  buffer[1]; // 0..126 
  if (tSpeed < 0) tSpeed = 0;
  if (tSpeed > 126) tSpeed = 126;
  int tDirection = (buffer[2] != 0); // 1 is forward, 0 is backward
  byte nB=0;

  if(cab>127)
    b[nB++]=highByte(cab) | 0xC0;      // convert train number into a two-byte address
    
  b[nB++]=lowByte(cab);
  b[nB++]=0x3F;                        // 128-step speed control byte
  if(tSpeed>=0) 
    // max speed is 126, but speed codes range from 2-127 (0=stop, 1=emergency stop)
    // so map 0, 1, 2 .. 126 -> to 0, 2, 3, .. 127 and set high bit iff forward
    b[nB++]=tSpeed+(tSpeed>0)+tDirection*128;
  else{
    b[nB++]=1; // emergency stop
    tSpeed=0;
  }

  Serial.print("set speed ");
  Serial.println(b[nB-1]);
  
  bufferPacket(slot, b, nB); // which will be silently ignored if refresh buffer has no room

  ack = 1; // success
}
 
// callback for sending data
void sendData(){
  // Wire.write(ack);
}

// DCC packets and refresh buffer

struct DCCPacket {
  byte buffer[10]; // max 76 bits
  byte bits;
};

byte idlePacket[3] = {0xFF, 0x00, 0x00};
byte testPacket[4] = {0x03, 0x3F, 0x9E, 0x00}; // just to test, set speed of loco 3 to 0x8F = 15 + 128 = 15 forward of 0x9E = 30 forward

struct RefreshSlot {
  DCCPacket packet[2];
  DCCPacket *activePacket;
  DCCPacket *updatePacket;
  bool inUse;
};

#define SLOTS 10
#define IMMEDIATE_QUEUE 10

struct RefreshBuffer {
  RefreshSlot refreshSlots[SLOTS]; // the refresh slots
  DCCPacket immediatePackets[IMMEDIATE_QUEUE];  // the queue for immediate packets
  byte outImmediate; // first packet to stream from immediateQueue
  byte inImmediate; // where to queue next immediateQueue packet, thus immediateQueue size = inImmediate - outImmediate
  // full is represented as inImmediate + 1 = outImmediate, wasting as single cell
  byte currentSlot; // [0..SLOTS-1] for refresh slot, SLOTS for immediate queue
  byte currentBit;
  DCCPacket *currentPacket;
};

volatile RefreshBuffer theBuffer;

void initRefreshBuffer() {
  // initially, all refresh slots are free
  for (int i=0; i<SLOTS; ++i) {
    theBuffer.refreshSlots[i].inUse = false;
    theBuffer.refreshSlots[i].packet[0].bits = 0;
    theBuffer.refreshSlots[i].packet[1].bits = 0;
    theBuffer.refreshSlots[i].activePacket = theBuffer.refreshSlots[i].packet; // first packet is active, after slot is taking into usage, of course
    theBuffer.refreshSlots[i].updatePacket = theBuffer.refreshSlots[i].packet + 1; // second is for update
    theBuffer.refreshSlots[i].activePacket->bits = 0;
    theBuffer.refreshSlots[i].updatePacket->bits = 0;
  }

  // and the immediate queue is empty
  theBuffer.outImmediate = 0;
  theBuffer.inImmediate = 0;

  // establish invariant, there is always something to stream in the refresh buffer
  // being it an immediate idle packet when starting up
  bufferImmediatePacket(idlePacket, 2);
  // DCCbytesToPacket(idlePacket, 2, theBuffer.reg[0].activePacket);
  // DCCbytesToPacket(testPacket, 3, theBuffer.reg.activePacket);

  // next to play is the idle packet in the immediate buffer
  theBuffer.currentSlot = SLOTS; 
  theBuffer.currentBit = 0;
  theBuffer.currentPacket = theBuffer.immediatePackets + theBuffer.outImmediate; // well, the latter being always zero here.
}

void bufferPacket(byte slot, byte in[], byte nBytes) {
  theBuffer.refreshSlots[slot].inUse = true; // TODO: check slot value
  if (theBuffer.refreshSlots[slot].updatePacket->bits != 0) return; // buffer full, for now, silently ignore
  DCCbytesToPacket(in, nBytes, theBuffer.refreshSlots[slot].updatePacket);
}

void bufferImmediatePacket(byte in[], byte nBytes) {
  byte next = (theBuffer.inImmediate + 1) % IMMEDIATE_QUEUE;
  if (next == theBuffer.outImmediate) return; // buffer full, for now, silently ignore
  DCCbytesToPacket(in, nBytes, theBuffer.immediatePackets  + theBuffer.inImmediate);
  theBuffer.inImmediate = next;
}

byte mask[] = {0x80,0x40,0x20,0x10,0x08,0x04,0x02,0x01};

boolean nextBit() {
  if (theBuffer.currentBit == theBuffer.currentPacket->bits) {  
    // done steaming the current packet, what is next?

    // look for the next slot to stream from
    byte oldCurrentSlot = theBuffer.currentSlot;

    // if we were doing a immediate packet, dequeue it now
    if (oldCurrentSlot == SLOTS) {
      theBuffer.outImmediate = (theBuffer.outImmediate + 1) % IMMEDIATE_QUEUE;
    }

    while (true) {
      theBuffer.currentSlot = (theBuffer.currentSlot + 1) % (SLOTS + 1);
      if (theBuffer.currentSlot == SLOTS) {
        // immediate queue is next, anything available?
        if (theBuffer.inImmediate != theBuffer.outImmediate) {
          theBuffer.currentPacket = theBuffer.immediatePackets + theBuffer.outImmediate;
          break; // found!
        }
      } else {
        // update slot is is next, is it in use?
        if (theBuffer.refreshSlots[theBuffer.currentSlot].inUse) {
          if (theBuffer.refreshSlots[theBuffer.currentSlot].updatePacket->bits != 0) {
            DCCPacket *tmp = theBuffer.refreshSlots[theBuffer.currentSlot].activePacket;
            theBuffer.refreshSlots[theBuffer.currentSlot].activePacket = theBuffer.refreshSlots[theBuffer.currentSlot].updatePacket;
            theBuffer.refreshSlots[theBuffer.currentSlot].updatePacket = tmp;
            // and clear what is now updatePacket
            theBuffer.refreshSlots[theBuffer.currentSlot].updatePacket->bits = 0;
          } 
          // and start streaming (new) activePacket or repeat to old activePacket or next buffer
          theBuffer.currentPacket = theBuffer.refreshSlots[theBuffer.currentSlot].activePacket;
          break; // found!
        }
      }
      if (theBuffer.currentSlot == oldCurrentSlot) {
        // we scanned all slots and immediate queue and nothing the buffer, maintain invariant by creating an idle packet
        bufferImmediatePacket(idlePacket, 2);
        // and make the it the currentPacket
        theBuffer.currentPacket = theBuffer.immediatePackets + theBuffer.outImmediate;
        theBuffer.currentSlot = SLOTS;
        break; // done!
      }
    }

    // start the newly found packet
    theBuffer.currentBit = 0;
  }

  boolean res = theBuffer.currentPacket->buffer[theBuffer.currentBit/8] & mask[theBuffer.currentBit%8];
  theBuffer.currentBit++;
  return res;
}

/*
 * 3 bytes:

22 preamble 
 1 packet start bit
 8 byte 1
 1 data start bit
 8 byte 2
 1 data start bit
 8 error detection (byte3)
 1 packet end bit

 */

void DCCbytesToPacket(byte in[], int nBytes,  struct DCCPacket *packet) {          
  byte *out = packet->buffer;
  in[nBytes]=in[0];                        // copy first byte into what will become the checksum byte  
  for(int i=1;i<nBytes;i++)              // XOR remaining bytes into checksum byte
    in[nBytes]^=in[i];
  nBytes++;                              // increment number of bytes in packet to include checksum byte
      
  out[0]=0xFF;                         // first 8 bits of 22-byte preamble
  out[1]=0xFF;                        // second 8 bits of 22-byte preamble
  out[2]=0xFC + bitRead(in[0],7);      // last 6 bits of 22-byte preamble + data start bit + b[0], bit 7
  out[3]=in[0]<<1;                     // in[0], bits 6-0 + data start bit
  out[4]=in[1];                        // in[1], all bits
  out[5]=in[2]>>1;                     // in[2], bits 7-1
  out[6]=in[2]<<7;                     // in[2], bit 0

  if (nBytes==3) {
    packet->bits=49;
  } else{
    out[6]+=in[3]>>2;                  // in[3], bits 7-2
    out[7]=in[3]<<6;                   // in[3], bit 1-0
    if(nBytes==4){
      packet->bits=58;
    } else{
      out[7]+=in[4]>>3;                // in[4], bits 7-3
      out[8]=in[4]<<5;                 // in[4], bits 2-0
      if(nBytes==5){
        packet->bits=67;
      } else{
        out[8]+=in[5]>>4;              // in[5], bits 7-4
        out[9]=in[5]<<4;               // in[5], bits 3-0
        packet->bits=76;
      } // >5 bytes
    } // >4 bytes
  } // >3 bytes
} 

// checking current on both main and prog tracks

#define CURRENT_SAMPLE_TIME        10
#define CURRENT_SAMPLE_SMOOTHING   0.01
#define CURRENT_SAMPLE_MAX         300

#define CURRENT_MONITOR_PIN_MAIN A0
#define CURRENT_MONITOR_PIN_PROG A1

#define CURRENT_MAIN 0
#define CURRENT_PROG 1
  
long int lastSampleTime = 0;

struct Power {
  float current[2] = {0.0, 0.0}; 
  boolean state; // requested state
  boolean error; // error
};

volatile struct Power power;

void initPower() {
  power.state = false;
  power.error = false;
  updatePowerView();
}

void powerOn() {
  power.state = true;
  power.error = false;
}

void powerOff() {
  power.state = false;
  power.error = false;
}

void powerError() {
  power.error = true;
  digitalWrite(SPEED_MOTOR_CHANNEL_PIN_A,LOW);                                                     // disable both Motor Shield Channels
  digitalWrite(SPEED_MOTOR_CHANNEL_PIN_B,LOW);                                                     // regardless of which caused current overload
}

void checkCurrent() {
  if (millis()-lastSampleTime<CURRENT_SAMPLE_TIME) return;
  check(CURRENT_MONITOR_PIN_MAIN, power.current + CURRENT_MAIN);
  check(CURRENT_MONITOR_PIN_PROG, power.current + CURRENT_PROG);
  lastSampleTime=millis();                                   // note millis() uses TIMER-0.  For UNO, we change the scale on Timer-0.  For MEGA we do not.  This means CURENT_SAMPLE_TIME is different for UNO then MEGA
}

void check(int pin, float *current) {
  (*current) = analogRead(pin)*CURRENT_SAMPLE_SMOOTHING+(*current)*(1.0-CURRENT_SAMPLE_SMOOTHING);        // compute new exponentially-smoothed current
  // Serial.print("power "); Serial.println(*current);
  if((*current)>CURRENT_SAMPLE_MAX) {
    Serial.println("overload, switch off");
    powerError();
  }    
}

void updatePowerView() {
  // TODO: signal error condition
  // led power = power.state; // on iff power on
  // led power error;
}
 
