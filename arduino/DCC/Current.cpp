#include "Current.h"

#define CURRENT_SAMPLE_TIME        10
#define CURRENT_SAMPLE_SMOOTHING   0.01
#define CURRENT_SAMPLE_MAX         675 // 2000mA

// Sample ranges from 0..1023 for measured values 0v..5v. Motorshield applies 3.3v at monitor pin at its max load of 2000mA (for single channel)
// So, unit is 1/1024*5/3.3 * 2000mA ~= 1.29 mA
// In other words, when we draw 2000mA, we expect a reading of 675/676
  
Current::Current() {
  // disable 'real' output pin of motor direction (drive through timer output instead using jumper wire)
  pinMode(DIRECTION_MOTOR_CHANNEL_PIN_A, INPUT);
  digitalWrite(DIRECTION_MOTOR_CHANNEL_PIN_A, LOW);
  pinMode(DIRECTION_MOTOR_CHANNEL_PIN_B, INPUT);
  digitalWrite(DIRECTION_MOTOR_CHANNEL_PIN_B, LOW);

  // pins drivings channel A
  pinMode(SPEED_MOTOR_CHANNEL_PIN_A, OUTPUT);  // main track power
  pinMode(BRAKE_MOTOR_CHANNEL_PIN_A, OUTPUT);  // main track brake

  // initialize values
  value[0] = 0.0; value[1] = 0.0; lastSampleTime = 0; 

  off(); 
}

void Current::on() {
  // power on
  digitalWrite(SPEED_MOTOR_CHANNEL_PIN_A, HIGH); // power on!
  digitalWrite(BRAKE_MOTOR_CHANNEL_PIN_A, LOW);  // release brake!

  /*   
  pinMode(SPEED_MOTOR_CHANNEL_PIN_B, OUTPUT);  // prog track power
  pinMode(BRAKE_MOTOR_CHANNEL_PIN_B, OUTPUT);  // prog track brake
  somehow BRAKE_B conflicts with LocoNet lib
  digitalWrite(SPEED_MOTOR_CHANNEL_PIN_B, HIGH);
  digitalWrite(BRAKE_MOTOR_CHANNEL_PIN_B, LOW);
  */
}

void Current::off() {
  // power off
  digitalWrite(SPEED_MOTOR_CHANNEL_PIN_A, LOW); // power off!
  digitalWrite(BRAKE_MOTOR_CHANNEL_PIN_A, HIGH);  // brake!
}

boolean Current::check(int pin, int index) {
  value[index] = analogRead(pin) * CURRENT_SAMPLE_SMOOTHING + value[index] * (1.0 - CURRENT_SAMPLE_SMOOTHING); // compute new exponentially-smoothed current
  if (value[index] > CURRENT_SAMPLE_MAX) {
    // short circuit, shut off
    off();
    return false;
  }
  return true;
}

boolean Current::check() {
  if (millis() - lastSampleTime < CURRENT_SAMPLE_TIME) return;
  boolean checkMain = check(CURRENT_MONITOR_PIN_MAIN, 0);
  boolean checkProg = check(CURRENT_MONITOR_PIN_PROG, 1);
  lastSampleTime = millis();  // note millis() uses TIMER-0, which we also change for DCC signal generation, so expect non-standard values
  return checkMain && checkProg;
}
