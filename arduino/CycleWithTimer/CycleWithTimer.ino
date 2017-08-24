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
  // main loop is empty
}

// ack data from receive to transmit
volatile byte ack;
 
// callback for received data
void receiveData(int byteCount) {
  Serial.println("start receive");
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

  if (count < 2) return; // no enough data
  // Just a basic interface to start:
  // cmd: direction value: 0..126 or 
  // data[0]: direction (0 is backwards, != 0 is forward

  // create a DCC update throttle command
  byte b[5]; // max 5 bytes, including checksum byte
  int cab = 3; // TODO: for now, hard coded loc 3
  int tSpeed =  buffer[0]; // 0..126 
  if (tSpeed < 0) tSpeed = 0;
  if (tSpeed > 126) tSpeed = 126;
  int tDirection = (buffer[1] != 0); // 1 is forward, 0 is backward
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

  Serial.print("set speed bytes : ");
  for (int i =0; i<nB; i++) {
    Serial.print(b[i]); Serial.print(" ");
  }
  Serial.println();

  updatePacket(b, nB); // which will be silently ignored if refresh buffer has no room

  ack = 1; // success
}
 
// callback for sending data
void sendData(){
  // Wire.write(ack);
}

// DCC packets and refresh buffer

struct DCCPacket {
  byte buffer[10];
  byte bits;
};

byte idlePacket[3]={0xFF,0x00,0};
byte testPacket[4] ={0x03, 0x3F, 0x9E}; // just to test, set speed of loco 3 to 0x8F = 15 + 128 = 15 forward of 0x9E = 30 forward

struct RefreshRegister {
  DCCPacket packet[2];
  DCCPacket *activePacket;
  DCCPacket *updatePacket;
};

// for now, a single register
struct RefreshBuffer {
  RefreshRegister reg;
  byte currentBit;
};

volatile RefreshBuffer theBuffer;

void initRefreshBuffer() {
  // init
  theBuffer.reg.packet[0].bits = 0;
  theBuffer.reg.packet[1].bits = 0;
  theBuffer.reg.activePacket = theBuffer.reg.packet; // first packet is active
  theBuffer.reg.updatePacket = theBuffer.reg.packet + 1; // second is for update

  theBuffer.reg.activePacket->bits = 0;
  theBuffer.reg.updatePacket->bits = 0;
  
  // load an idle packet into activePacket
  DCCbytesToPacket(idlePacket, 2, theBuffer.reg.activePacket);
  // DCCbytesToPacket(testPacket, 3, theBuffer.reg.activePacket);

  // next bit to play is first bit of activePacket
  theBuffer.currentBit = 0;
}

void updatePacket(byte in[], byte nBytes) {
  if (theBuffer.reg.updatePacket->bits != 0) return; // buffer full, for now, silently ignore
  DCCbytesToPacket(in, nBytes, theBuffer.reg.updatePacket);
}

byte mask[] = {0x80,0x40,0x20,0x10,0x08,0x04,0x02,0x01};

boolean nextBit() {
  if (theBuffer.currentBit == theBuffer.reg.activePacket->bits) {  
    // all bit is activePacket are done, copy next packet if available
    if (theBuffer.reg.updatePacket->bits != 0) {
      Serial.println("load new packet");
      DCCPacket *tmp = theBuffer.reg.activePacket;
      theBuffer.reg.activePacket = theBuffer.reg.updatePacket;
      theBuffer.reg.updatePacket = tmp;
      // and clear what is now updatePacket
      theBuffer.reg.updatePacket->bits = 0;
    } 
    // and start streaming (new) activePacket or repeat to old activePacket
    theBuffer.currentBit = 0;
  }

  boolean res = theBuffer.reg.activePacket->buffer[theBuffer.currentBit/8] & mask[theBuffer.currentBit%8];
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
