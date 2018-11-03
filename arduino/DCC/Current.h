#pragma once

#include <Arduino.h>
#include "Config.h"

//// monitor/control/check current

class Current {
  public:
    Current();
    void on();
    void off();
    boolean check(); // return 'false' if overload detected and circuit has been switched off
  private:
    boolean check(int pin, int index); // return 'false' if overload detected and circuit has been switched off
    float value[2]; // 0 is main, 1 is programming
    long int lastSampleTime;
};



