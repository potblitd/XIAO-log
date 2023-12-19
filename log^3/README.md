# logloglog

Seeking compactness and all-in-one-ness, a shield was deisgned for the XIAO ESP32C3, that includes the previously used SHT40 temperature sensor, the popular BH1750 ambient light sensor and the PCF8563 RTC chip to correct the awful internal clock. 

## Materials


<p align="center">
  <img src="images/DSC02819-crop-nasic.JPG" width="500" />
</p>

## Assembly

Instead of soldering the battery directly onto the chip, a connector is fixed to facilitate removal and power OFF. The sensor uses I2C protocol which can be configured on pins D4 (SDA) and D5 (SCL). By connecting the power supply for the sensor on a GPIO, such as D10, it can be powered only when needed and therefore reduce the power consumption during the deep sleep phase.



## Code

The easiest way to get micropython on the ESP32C3 chip is to download the firmware [here](https://micropython.org/download/esp32c3-usb/), enter bootloader mode by holding the BOOT button down while pressing the RESET button, and then flash the chip with the bin file using [esptool](https://docs.espressif.com/projects/esptool/en/latest/esp32/) through the command prompt at the right COM port. With the [Thonny IDE](https://thonny.org/), the onboard files can easily be managed, and codes can be run directly without uploading them. The example code below is simple : it gets the RTC time, reads temperature and humidity from the sensor, writes all these values in a file and then calculates the time until the next measurement based on the log period before going into deep sleep.

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




