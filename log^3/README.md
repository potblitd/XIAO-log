# logloglog

Seeking compactness and all-in-one-ness, a logger shield was deisgned for the XIAO ESP32C3. It includes the previously used SHT40 temperature sensor, the popular BH1750 ambient light sensor and the PCF8563 RTC chip to correct the godawful internal clock.

<p align="center">
  <img src="img.png" width="500" />
</p>

## Assembly

Just solder the components on the right footprint, not much to explain here. I squeezed a 100mAh Li-ion battery between the two PCB boards

## Code

set_rtc.py can be used to set the time on PCF8563 form the local time on the PC.

xiaologhot.py is The example code below is simple : it gets the RTC time, reads temperature and humidity from the sensor, writes all these values in a file and then calculates the time until the next measurement based on the log period before going into deep sleep.

```ruby
# PARAMETERS
log_period = 60
 
# LIBRAIRIES
import os, time, machine
 
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
 
# VALUES LOG
if not "log.csv" in os.listdir():
    with open("log.csv", "a") as log:
        log.write("date,time,temp,hum\n")
with open("log.csv", "a") as log:
    log.write(now_date+","+now_time+","+str(temp)+","+str(humi)+"\n")
 
# DEEP SLEEP TIME
now_second = now[4]*60 + now[5]
sleep_time = log_period - (now_second % log_period)
print("Going to sleep for %0.2f minutes" % (sleep_time/60))
machine.deepsleep(sleep_time*1000)
# end
```

Currently, the only way to connect the ESP32C3 board while in deep sleep for retrieving the log file, is to physically reset the board and quickly click the restart backend button before the code arrives at the deep sleep again.

## Performance

The current consumption during the measurements and logging is about 20mA and takes about 2 seconds. In deep sleep, the system draws 42uA from the battery which is *nice* considering the RTC time is still running. So, with the 1000mAh battery from this setup, the loggers can theoretically run for 58 days with 60 seconds log period. To verify this mad claim, here is a graph of the battery level during this logging run.

*[coming in 58 days]*





