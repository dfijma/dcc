#! /usr/bin/python
import Adafruit_GPIO.FT232H as FT232H

# Temporarily disable FTDI serial drivers.
# FT232H.use_FT232H()

# Find the first FT232H device.
ft232h = FT232H.FT232H()

# Create an I2C device at address 0x70.
a = None
i2c = FT232H.I2CDevice(ft232h, 0x04)
#ii2c.writeList(0x02, [1,2,3,4])
# a=i2c.readList(37, 32)
#a=i2c.readU8(42)
a=i2c.readRaw8()
#i2c.write8(42,37)
print(a)
