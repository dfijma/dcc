from pad4pi import rpi_gpio
from time import sleep
import queue
import threading

q = queue.LifoQueue()

KEYPAD = [
    [1, 2, 3],
    [4, 5, 6],
    [7, 8, 9],
    ["*", 0, "#"]
]

ROW_PINS = [26, 19, 13, 6] # BCM numbering
COL_PINS = [21, 20, 16] # BCM numbering

factory = rpi_gpio.KeypadFactory()

# Try factory.create_4_by_3_keypad
# and factory.create_4_by_4_keypad for reasonable defaults
keypad = factory.create_keypad(keypad=KEYPAD, row_pins=ROW_PINS, col_pins=COL_PINS)

def printKey(key):
    q.put(key)

# printKey will be called each time a keypad button is pressed
keypad.registerKeyPressHandler(printKey)

def timeout():
    q.put("xxx")

t = None

def schedule():
    global t
    t = threading.Timer(2.0, timeout)
    t.start()

schedule()

while True:
    item = q.get()
    print(item)
    if item == "xxx":
        schedule()
    if item == 0:
        break
keypad.cleanup()
