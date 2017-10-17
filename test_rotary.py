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
    print (event)
    if event == RotaryEncoder.CLOCKWISE:
        value = min(value + 1, 126)
        print value
    elif event == RotaryEncoder.ANTICLOCKWISE:
        value = max(value - 1, 0)
        print value
    elif event == RotaryEncoder.BUTTONDOWN:
         print "Button down"
    elif event == RotaryEncoder.BUTTONUP:
        print "Button up"
    return
# Define the switch
#rswitch = RotaryEncoder(PIN_A,PIN_B,BUTTON,switch_event)
duco = RotaryEncoder(17,27, 22, switch_event)
#while True:
#     time.sleep(0.5)
duco.step(0,1)
duco.step(1,1)
duco.step(1,0)
duco.step(0,0)
duco.step(0,1)
duco.step(1,0)
duco.step(0,0)
duco.step(0,1)
duco.step(1,1)
duco.step(0,0)
