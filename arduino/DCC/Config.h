#pragma once

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


#define CURRENT_MONITOR_PIN_MAIN A0
#define CURRENT_MONITOR_PIN_PROG A1


//// i2c 
#define SLAVE_ADDRESS 0x04


