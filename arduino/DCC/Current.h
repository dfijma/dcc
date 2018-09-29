#pragma once

class Current {
  public:
    Current();
    void on();
    void off();
    void check();
  private:
    void check(int pin, int index);
    float value[2]; // 0 is main, 1 is programming
    long int lastSampleTime;
};

