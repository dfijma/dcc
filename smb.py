from gpiozero import LED, Button
from time import sleep
from signal import pause
from rotary_switch import RotaryEncoder

import Adafruit_CharLCD as LCD
import Adafruit_GPIO.MCP230xx as MCP
import time
import math

import smbus

bus = smbus.SMBus(1)
address = 0x04


def f():
    for i in range(0, 10):   
        bus.write_i2c_block_data(address, 0x01, [i])
#        result = bus.read_i2c_block_data(address,  0x04, 1)
        result = bus.read_word_data(address, 0x4)
        print("{}".format(result))
        sleep(0.01)

f()
