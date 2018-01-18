#include <Wire.h>
#include <LocoNet.h>

// loconet monitor

lnMsg        *LnPacket;

// DCC packets and refresh buffer

struct DCCPacket {
  byte buffer[10]; // max 76 bits
  byte bits;
};

static byte idlePacket[3] = {0xFF, 0x00, 0x00}; // 2 bytes, 0 terminated

// TODO: wire this to enable digital loc functions
int createFunctionPacket(byte buffer[], int cab, byte b) {
  // high byte cab, low byte cab, 1100 f4f3f2f1 (from byte b)
  int nB = 0;
  buffer[nB++] = highByte(cab);
  buffer[nB++] = lowByte(cab);
  buffer[nB++] = (b | 0x80); // & 0xBF; // always of form 10xx xxxx
  return nB;
}

struct RefreshSlot {
  DCCPacket packet[2];
  DCCPacket *activePacket;
  DCCPacket *updatePacket;
  bool inUse;
};


//#define SLOTS 2
//#define IMMEDIATE_QUEUE 4

struct RefreshBuffer {
  byte slots;
  byte immediate_queue_size;
  RefreshSlot *refreshSlots; // the refresh slots
  DCCPacket *immediatePackets;  // the queue for immediate packets
  byte outImmediate; // first packet to stream from immediateQueue
  byte inImmediate; // where to queue next immediateQueue packet, thus immediateQueue size = inImmediate - outImmediate
  // full is represented as inImmediate + 1 = outImmediate, wasting as single cell
  byte currentSlot; // [0..SLOTS-1] for refresh slot, SLOTS for immediate queue
  byte currentBit;
  DCCPacket *currentPacket;
};

void initRefreshBuffer(struct RefreshBuffer *buffer, byte slots, byte immediate_queue_size) {
  buffer->slots = slots;
  buffer->immediate_queue_size = immediate_queue_size;
  buffer->refreshSlots = (RefreshSlot*) malloc(sizeof(RefreshSlot) * slots);
  buffer->immediatePackets = (DCCPacket*) malloc(sizeof(DCCPacket) * immediate_queue_size);
  // initially, all refresh slots are free
  for (int i = 0; i < buffer->slots; ++i) {
    buffer->refreshSlots[i].inUse = false;
    buffer->refreshSlots[i].packet[0].bits = 0;
    buffer->refreshSlots[i].packet[1].bits = 0;
    buffer->refreshSlots[i].activePacket = buffer->refreshSlots[i].packet; // first packet is active, after slot is taking into usage, of course
    buffer->refreshSlots[i].updatePacket = buffer->refreshSlots[i].packet + 1; // second is for update
    buffer->refreshSlots[i].activePacket->bits = 0;
    buffer->refreshSlots[i].updatePacket->bits = 0;
  }

  // and the immediate queue is empty
  buffer->outImmediate = 0;
  buffer->inImmediate = 0;

  // establish invariant, there is always something to stream in the refresh buffer
  // being it an immediate idle packet when starting up
  bufferImmediatePacket(buffer, idlePacket, 2);

  // next to play is the idle packet in the immediate buffer
  buffer->currentSlot = buffer->slots;
  buffer->currentBit = 0;
  buffer->currentPacket = buffer->immediatePackets + buffer->outImmediate; // well, the latter being always zero here.
}

void bufferPacket(struct RefreshBuffer *buffer, byte slot, byte in[], int nBytes) {
  buffer->refreshSlots[slot].inUse = true; // TODO: check slot value
  if (buffer->refreshSlots[slot].updatePacket->bits != 0) return; // buffer full, for now, silently ignore
  DCCbytesToPacket(in, nBytes, buffer->refreshSlots[slot].updatePacket);
}

void bufferImmediatePacket(struct RefreshBuffer *buffer, byte in[], int nBytes) {
  byte next = (buffer->inImmediate + 1) % (buffer->immediate_queue_size);
  if (next == buffer->outImmediate) return; // buffer full, for now, silently ignore
  DCCbytesToPacket(in, nBytes, buffer->immediatePackets  + buffer->inImmediate);
  buffer->inImmediate = next;
}

static byte mask[] = {0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01};

boolean nextBit(RefreshBuffer *buffer) {
  if (buffer->currentBit ==  buffer->currentPacket->bits) {
    // done steaming the current packet, what is next?

    // look for the next slot to stream from
    byte oldCurrentSlot = buffer->currentSlot;

    // if we were doing a immediate packet, dequeue it now
    if (oldCurrentSlot == buffer->slots) {
      buffer->outImmediate = (buffer->outImmediate + 1) % (buffer->immediate_queue_size);
    }

    while (true) {
      buffer->currentSlot = (buffer->currentSlot + 1) % (buffer->slots + 1);
      if (buffer->currentSlot == buffer->slots) {
        // immediate queue is next, anything available?
        if (buffer->inImmediate != buffer->outImmediate) {
          buffer->currentPacket = buffer->immediatePackets + buffer->outImmediate;
          break; // found!
        }
      } else {
        // update slot is is next, is it in use?
        if (buffer->refreshSlots[buffer->currentSlot].inUse) {
          if (buffer->refreshSlots[buffer->currentSlot].updatePacket->bits != 0) {
            DCCPacket *tmp = buffer->refreshSlots[buffer->currentSlot].activePacket;
            buffer->refreshSlots[buffer->currentSlot].activePacket = buffer->refreshSlots[buffer->currentSlot].updatePacket;
            buffer->refreshSlots[buffer->currentSlot].updatePacket = tmp;
            // and clear what is now updatePacket
            buffer->refreshSlots[buffer->currentSlot].updatePacket->bits = 0;
          }
          // and start streaming (new) activePacket or repeat to old activePacket or next buffer
          buffer->currentPacket = buffer->refreshSlots[buffer->currentSlot].activePacket;
          break; // found!
        }
      }
      if (buffer->currentSlot == oldCurrentSlot) {
        // we scanned all slots and immediate queue and nothing the buffer, maintain invariant by creating an idle packet
        bufferImmediatePacket(buffer, idlePacket, 2);

        // and make the it the currentPacket
        buffer->currentPacket = buffer->immediatePackets + buffer->outImmediate;
        buffer->currentSlot = buffer->slots;
        break; // done!
      }
    }

    // start the newly found packet
    buffer->currentBit = 0;
  }

  boolean res = buffer->currentPacket->buffer[buffer->currentBit / 8] & mask[buffer->currentBit % 8];
  buffer->currentBit++;
  return res;
}

volatile RefreshBuffer mainBuffer;
volatile RefreshBuffer progBuffer;

// i2c used for input
#define SLAVE_ADDRESS 0x04

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

void setup() {
  // serial
  Serial.begin(57600);

  // loconet 
  LocoNet.init();

  // initialize refresh buffers (with an idle packet)
  initRefreshBuffer(&mainBuffer, 10, 3);
  initRefreshBuffer(&progBuffer, 2, 3);

  // initialize i2c as slave
  Wire.begin(SLAVE_ADDRESS);

  // define callbacks for i2c input
  Wire.onReceive(receiveData);
  Wire.onRequest(sendData);

  // disable 'real' output pin of motor direction (drive through timer output instead using jumper wire)
  pinMode(DIRECTION_MOTOR_CHANNEL_PIN_A, INPUT);
  digitalWrite(DIRECTION_MOTOR_CHANNEL_PIN_A, LOW);
  pinMode(DCC_SIGNAL_PIN_MAIN, OUTPUT);
  pinMode(DIRECTION_MOTOR_CHANNEL_PIN_B, INPUT);
  digitalWrite(DIRECTION_MOTOR_CHANNEL_PIN_B, LOW);
  pinMode(DCC_SIGNAL_PIN_PROG, OUTPUT);

  //bitClear(PRR, PRTIM0);
  //bitClear(PRR, PRTIM2);

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

  // power on
  pinMode(SPEED_MOTOR_CHANNEL_PIN_A, OUTPUT);  // main track power
  pinMode(BRAKE_MOTOR_CHANNEL_PIN_A, OUTPUT);  // main track brake
  digitalWrite(SPEED_MOTOR_CHANNEL_PIN_A, HIGH); // power on!
  digitalWrite(BRAKE_MOTOR_CHANNEL_PIN_A, LOW);  // release brake!

  pinMode(SPEED_MOTOR_CHANNEL_PIN_B, OUTPUT);  // prog track power
  pinMode(BRAKE_MOTOR_CHANNEL_PIN_B, OUTPUT);  // prog track brake
  /* 
  digitalWrite(SPEED_MOTOR_CHANNEL_PIN_B, HIGH);
  digitalWrite(BRAKE_MOTOR_CHANNEL_PIN_B, LOW);
  */
}

ISR(TIMER0_COMPB_vect) {
  // actual DCC output is handled by timer hardware (flipping from LOW to HIGH at half cycle)
  // when handling the interrupt itself, the only way we need to do is reconfigure timer for next cycle
  // we can reconfigure timer for next cycle while current cycle is only half way because the relevant registers
  // are double latched (and only really loaded in the actual registers at next time timer flows over from TOP to BOTTOM
  // next bit, please
  if (nextBit(&mainBuffer)) {
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

void loop() {
  // check current
  checkCurrent();

  // loconet
    // Check for any received LocoNet packets
  LnPacket = LocoNet.receive() ;
  if ( LnPacket ) {
    // First print out the packet in HEX
    Serial.print("RX: ");
    uint8_t msgLen = getLnMsgSize(LnPacket);
    for (uint8_t x = 0; x < msgLen; x++)
    {
      uint8_t val = LnPacket->data[x];
      // Print a leading 0 if less than 16 to make 2 HEX digits
      if (val < 16)
        Serial.print('0');

      Serial.print(val, HEX);
      Serial.print(' ');
    }

    // If this packet was not a Switch or Sensor Message then print a new line
    if (!LocoNet.processSwitchSensorMessage(LnPacket)) {
      Serial.println();
    }
  } 
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
  for (int i = 0; i < count; i++) {
    Serial.print(buffer[i]);
    Serial.print(" ");
  }
  Serial.println();

  if (count < 2) return; // no enough data
  // Just a basic interface to start:
  // cmd: slot
  // data[0]: direction value: 0..126 or
  // data[1]: direction (0 is backwards, != 0 is forward

  // create a DCC update throttle command
  byte slot = 1; // for now, hardcoded
  byte b[5]; // max 5 bytes, including checksum byte
  int cab = 3; // TODO: for now, hard coded loc 3
  int tSpeed =  buffer[0]; // 0..126
  if (tSpeed < 0) tSpeed = 0;
  if (tSpeed > 126) tSpeed = 126;
  int tDirection = (buffer[1] != 0); // 1 is forward, 0 is backward
  byte nB = 0;

  if (cab > 127)
    b[nB++] = highByte(cab) | 0xC0;    // convert train number into a two-byte address

  b[nB++] = lowByte(cab);
  b[nB++] = 0x3F;                      // 128-step speed control byte
  if (tSpeed >= 0)
    // max speed is 126, but speed codes range from 2-127 (0=stop, 1=emergency stop)
    // so map 0, 1, 2 .. 126 -> to 0, 2, 3, .. 127 and set high bit iff forward
    b[nB++] = tSpeed + (tSpeed > 0) + tDirection * 128;
  else {
    b[nB++] = 1; // emergency stop
    tSpeed = 0;
  }

  Serial.print("set speed ");
  Serial.println(b[nB - 1]);

  bufferPacket(&mainBuffer, slot, b, nB); // which will be silently ignored if refresh buffer has no room

  ack = 1; // success
}

// callback for sending data
void sendData() {
  // Wire.write(ack);
}



/*
   3 bytes:

  22 preamble
  1 packet start bit
  8 byte 1
  1 data start bit
  8 byte 2
  1 data start bit
  8 error detection (byte3)
  1 packet end bit

*/


// in should be of length nBytes + 1, that is, have room for checksum byte
void DCCbytesToPacket(byte in[], int nBytes, struct DCCPacket *packet) {
  byte *out = packet->buffer;
  in[nBytes] = in[0];                      // copy first byte into what will become the checksum byte
  for (int i = 1; i < nBytes; i++) {      // XOR remaining bytes into checksum byte
    in[nBytes] ^= in[i];
  }
  nBytes++;                              // increment number of bytes in packet to include checksum byte

  out[0] = 0xFF;                       // first 8 bits of 22-byte preamble
  out[1] = 0xFF;                      // second 8 bits of 22-byte preamble
  out[2] = 0xFC + bitRead(in[0], 7);   // last 6 bits of 22-byte preamble + data start bit + b[0], bit 7
  out[3] = in[0] << 1;                 // in[0], bits 6-0 + data start bit
  out[4] = in[1];                      // in[1], all bits
  out[5] = in[2] >> 1;                 // in[2], bits 7-1
  out[6] = in[2] << 7;                 // in[2], bit 0

  if (nBytes == 3) {
    packet->bits = 49;
  } else {
    out[6] += in[3] >> 2;              // in[3], bits 7-2
    out[7] = in[3] << 6;               // in[3], bit 1-0
    if (nBytes == 4) {
      packet->bits = 58;
    } else {
      out[7] += in[4] >> 3;            // in[4], bits 7-3
      out[8] = in[4] << 5;             // in[4], bits 2-0
      if (nBytes == 5) {
        packet->bits = 67;
      } else {
        out[8] += in[5] >> 4;          // in[5], bits 7-4
        out[9] = in[5] << 4;           // in[5], bits 3-0
        packet->bits = 76;
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
  digitalWrite(SPEED_MOTOR_CHANNEL_PIN_A, LOW);                                                    // disable both Motor Shield Channels
  digitalWrite(SPEED_MOTOR_CHANNEL_PIN_B, LOW);                                                    // regardless of which caused current overload
}

void checkCurrent() {
  if (millis() - lastSampleTime < CURRENT_SAMPLE_TIME) return;
  check(CURRENT_MONITOR_PIN_MAIN, power.current + CURRENT_MAIN);
  check(CURRENT_MONITOR_PIN_PROG, power.current + CURRENT_PROG);
  lastSampleTime = millis();                                 // note millis() uses TIMER-0.  For UNO, we change the scale on Timer-0.  For MEGA we do not.  This means CURENT_SAMPLE_TIME is different for UNO then MEGA
}

void check(int pin, float *current) {
  (*current) = analogRead(pin) * CURRENT_SAMPLE_SMOOTHING + (*current) * (1.0 - CURRENT_SAMPLE_SMOOTHING); // compute new exponentially-smoothed current
  // Serial.print("power "); Serial.println(*current);
  if ((*current) > CURRENT_SAMPLE_MAX) {
    Serial.println("overload, switch off");
    powerError();
  }
}

void updatePowerView() {
  // TODO: signal error condition
  // led power = power.state; // on iff power on
  // led power error;
}

// loconet
// This call-back function is called from LocoNet.processSwitchSensorMessage
// for all Sensor messages
void notifySensor( uint16_t Address, uint8_t State ) {
  Serial.print("Sensor: ");
  Serial.print(Address, DEC);
  Serial.print(" - ");
  Serial.println( State ? "Active" : "Inactive" );
}

// This call-back function is called from LocoNet.processSwitchSensorMessage
// for all Switch Request messages
void notifySwitchRequest( uint16_t Address, uint8_t Output, uint8_t Direction ) {
  Serial.print("Switch Request: ");
  Serial.print(Address, DEC);
  Serial.print(':');
  Serial.print(Direction ? "Closed" : "Thrown");
  Serial.print(" - ");
  Serial.println(Output ? "On" : "Off");
}

// This call-back function is called from LocoNet.processSwitchSensorMessage
// for all Switch Output Report messages
void notifySwitchOutputsReport( uint16_t Address, uint8_t ClosedOutput, uint8_t ThrownOutput) {
  Serial.print("Switch Outputs Report: ");
  Serial.print(Address, DEC);
  Serial.print(": Closed - ");
  Serial.print(ClosedOutput ? "On" : "Off");
  Serial.print(": Thrown - ");
  Serial.println(ThrownOutput ? "On" : "Off");
}

// This call-back function is called from LocoNet.processSwitchSensorMessage
// for all Switch Sensor Report messages
void notifySwitchReport( uint16_t Address, uint8_t State, uint8_t Sensor ) {
  Serial.print("Switch Sensor Report: ");
  Serial.print(Address, DEC);
  Serial.print(':');
  Serial.print(Sensor ? "Switch" : "Aux");
  Serial.print(" - ");
  Serial.println( State ? "Active" : "Inactive" );
}

// This call-back function is called from LocoNet.processSwitchSensorMessage
// for all Switch State messages
void notifySwitchState( uint16_t Address, uint8_t Output, uint8_t Direction ) {
  Serial.print("Switch State: ");
  Serial.print(Address, DEC);
  Serial.print(':');
  Serial.print(Direction ? "Closed" : "Thrown");
  Serial.print(" - ");
  Serial.println(Output ? "On" : "Off");
}

