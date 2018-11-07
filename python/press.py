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

def write(speed, direction):
    print("set speed " + str(speed) + " direction " + str(direction))
    dirval=0
    if direction:
        dirval=1
    data = [dirval]
    bus.write_i2c_block_data(address, speed, data)

def read():
    #result = bus.read_i2c_block_data(address,0x43)
    #print result
    pass

# Define MCP pins connected to the LCD.
lcd_rs        = 1
lcd_en        = 2
lcd_d4        = 3
lcd_d5        = 4
lcd_d6        = 5
lcd_d7        = 6
lcd_back      = 7

# Define LCD column and row size for 16x2 LCD.
lcd_columns = 16
lcd_rows    = 2

class TextView:

    def __init__(self, model):
        self.model = model
        self.redraw()

    def update(self):
        print(self.model.value)

    def redraw(self):
        self.update()

class View:
    
    def __init__(self,model):
        # Initialize MCP23017 device using its default 0x20 I2C address.
        self.gpio = MCP.MCP23008(0x20, busnum=1)
        self.lcd = LCD.Adafruit_CharLCD(lcd_rs, lcd_en, lcd_d4, lcd_d5, lcd_d6, lcd_d7,
                              lcd_columns, lcd_rows, lcd_back,
                              gpio=self.gpio, invert_polarity=False)
        self.led = LED(16)
        self.model = model
        self.formatter = "{:" + str(8) + "}"
        self.redraw()

    def update(self):
        self.lcd.set_cursor(0,1)
        self.lcd.message(self.formatter.format(str(self.model.value)))
        self.lcd.set_cursor(8,1)
        if self.model.toggle:
            self.lcd.message("forward ")
            self.led.on()
        else:
            self.lcd.message("backward")
            self.led.off()
        
    def redraw(self):
        self.lcd.clear()
        self.lcd.message("speed:  dir:")
        self.update()

class Model:
    def __init__(self):
        self.value = 0
        self.toggle = True

    def inc(self,x):
        self.value += x
        if self.value > 126:
            self.value = 126
        write(self.value, self.toggle)
        # read()

    def dec(self,x):
        self.value -= x
        if self.value < 0:
            self.value = 0
        write(self.value, self.toggle)
        # read()    

    def switch(self):
        print("SWITCH")
        self.toggle = not(self.toggle)
        # if self.value == 0:
        #    self.toggle = not(self.toggle)
        #else:
        #    pass
        #    # self.value=0
        write(self.value, self.toggle)
        # read()

# Define GPIO inputs
PIN_A = 17 # Pin 8
PIN_B = 27 # Pin 10
BUTTON = 22 # Pin 7

UP=1
DOWN=2

class Controller:

    def __init__(self,model,view):
        self.model = model
        self.view = view
        self.rswitch = RotaryEncoder(PIN_A, PIN_B, BUTTON, self.switch_event)
        self.state = UP

    def switch_event(self, event):
        if event == RotaryEncoder.CLOCKWISE:
            model.inc(1)
            view.update()
        elif event == RotaryEncoder.ANTICLOCKWISE:
            model.dec(1)
            view.update()
        elif event == RotaryEncoder.BUTTONDOWN:
            if self.state == UP:
                print("DOWN")
                model.switch()
            else:
                print("DOWN IGNORED")
            self.state = DOWN
            view.update()
        elif event == RotaryEncoder.BUTTONUP:
            self.state = UP

model = Model()
view = View(model)
controller = Controller(model, view)
    
pause()
