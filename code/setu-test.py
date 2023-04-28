# LIBRAIRIES
import os, time, machine, esp32

# DATE & TIME
now = time.localtime()
now_date = "-".join(map(str, now[0:3]))
now_time = ":".join(map(str, now[3:6]))
print("Date and time:", now_date, now_time)

# TEMPERATURE & HUMIDITY
sht40addr = 0x44
highprecision = 0XFD
i2c = machine.SoftI2C(sda=machine.Pin(6),scl=machine.Pin(7))
machine.Pin(10, machine.Pin.OUT).on()
time.sleep_ms(1)
i2c.writeto(sht40addr, bytes([highprecision]))
time.sleep_ms(10)
buf = i2c.readfrom(sht40addr, 6)
machine.Pin(10, machine.Pin.OUT).off()
temp = -45 + 175 * (buf[0]*256 + buf[1]) / 65535
humi = -6 + 125 * (buf[3]*256 + buf[4]) / 65535
print("Temperature: %0.1f C" % temp)
print("Humidity: %0.1f %%" % humi)