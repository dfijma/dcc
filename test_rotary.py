#!/usr/bin/env python
#
# Raspberry Pi Rotary Test Encoder Class
#
# Author : Bob Rathbone
# Site : http://www.bobrathbone.com
#
# This class uses a standard rotary encoder with push switch
#
import sys
import time
from my_rotary_switch import RotaryEncoder
# Define GPIO inputs
PIN_A = 17 # Pin 8
PIN_B = 27 # Pin 10
BUTTON = 22 # Pin 7
# This is the event callback routine to handle events
value = 0
def switch_event(event):
    global value
    # print ("event: " + str(event))
    if event == RotaryEncoder.CLOCKWISE:
        value = min(value + 1, 126)
    elif event == RotaryEncoder.ANTICLOCKWISE:
        value = max(value - 1, 0)
    elif event == RotaryEncoder.BUTTONDOWN:
         print "Button down"
    elif event == RotaryEncoder.BUTTONUP:
        print "Button up"
    print("value: " + str(value))
    return

# Define the switch
#rswitch = RotaryEncoder(PIN_A,PIN_B,BUTTON,switch_event)
rswitch = RotaryEncoder(17,27, 22, switch_event)
while True:
     time.sleep(0.5)

#right
rswitch.step(1,0)
rswitch.step(1,1)
rswitch.step(0,1)
rswitch.step(0,0)

#right
rswitch.step(1,0)
rswitch.step(1,1)
rswitch.step(0,1)
rswitch.step(0,0)

#right
rswitch.step(1,0)
rswitch.step(1,1)
rswitch.step(0,1)
rswitch.step(0,0)

#right with 2nd step missing
rswitch.step(1,0)
rswitch.step(0,1)
rswitch.step(0,0)

#right with 3rd step missing
rswitch.step(1,0)
rswitch.step(1,1)
rswitch.step(0,0)

#right with 4th step missing
rswitch.step(1,0)
rswitch.step(1,1)
rswitch.step(0,1)

#right
rswitch.step(1,0)
rswitch.step(1,1)
rswitch.step(0,1)
rswitch.step(0,0)
