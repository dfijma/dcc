#include "Arduino.h"
#include "Config.h"
#include "Current.h"

#define CURRENT_SAMPLE_TIME        10
#define CURRENT_SAMPLE_SMOOTHING   0.01
#define CURRENT_SAMPLE_MAX         300

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

  off(); //
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

void Current::check(int pin, int index) {
  value[index] = analogRead(pin) * CURRENT_SAMPLE_SMOOTHING + value[index] * (1.0 - CURRENT_SAMPLE_SMOOTHING); // compute new exponentially-smoothed current
  if (value[index] > CURRENT_SAMPLE_MAX) {
    Serial.println("overload, switch off");
    off();
  }
}

void Current::check() {
  if (millis() - lastSampleTime < CURRENT_SAMPLE_TIME) return;
  check(CURRENT_MONITOR_PIN_MAIN, 0);
  check(CURRENT_MONITOR_PIN_PROG, 1);
  lastSampleTime = millis();                                 // note millis() uses TIMER-0.  For UNO, we change the scale on Timer-0.  For MEGA we do not.  This means CURENT_SAMPLE_TIME is different for UNO then MEGA
  // Serial.print(lastSampleTime); Serial.print(" "); Serial.println(value[0]);
}

