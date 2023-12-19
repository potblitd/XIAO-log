# PARAMETERS
log_period = 60*30
test = True
wifi_update = True
ssid = 'EcoNet2023'
password = 'Ecoeco12345678'
url = 'https://api.thingspeak.com/update?api_key=ISDQ87W6S6DTIA5P'

# LIBRAIRIES
import os, time, machine, sys
if wifi_update: import network, urequests

# CONNECTION ON RESET
if machine.reset_cause() == 1: time.sleep_ms(10000)

# I2C INIT
i2c = machine.SoftI2C(sda = machine.Pin(6), scl = machine.Pin(7))

machine.Pin(10, machine.Pin.OUT).on()
time.sleep_ms(1)
print('I2C adresses currently on the lines :',i2c.scan())

# PCF8563 - DATE & TIME
# get time from external RTC
rtc_masks = [0x7F,0x7F,0x3F,0x3F,0x07,0x1F,0xFF]
rtc = [None]*7
for i in range(0,7):
    bcd = i2c.readfrom_mem(0x51, i+2, 1)[0] & rtc_masks[i]
    rtc[i] = (((bcd & 0xf0) >> 4) * 10 + (bcd & 0x0f))
    
rtc_date = str(rtc[6]+2000)+"-"+str(rtc[5])+"-"+str(rtc[3])
rtc_time = str(rtc[2])+":"+str(rtc[1])+":"+str(rtc[0])
print("Date and time:", rtc_date, rtc_time)

# update internal RTC
machine.RTC().datetime((rtc[6]+2000,rtc[5],rtc[3],rtc[4],rtc[2],rtc[1],rtc[0],1))

# SHT40 - TEMPERATURE & HUMIDITY
time.sleep_ms(1)
i2c.writeto(0x44, bytes([0xFD])) # set hi-res mode
time.sleep_ms(10)
buf = i2c.readfrom(0x44, 6)
temp = round(-45 + 175 * (buf[0]<<8 | buf[1]) / 65535, 3)
humi = round(-6 + 125 * (buf[3]<<8 | buf[4]) / 65535, 3)
print("Temperature: %0.2f C" % temp)
print("Humidity: %0.2f %%" % humi)

# BH1750 - LUMINOSITY
time.sleep_ms(1)
i2c.writeto(0x23, bytes([0x01])) # power on
i2c.writeto(0x23, bytes([0x07])) # reset
i2c.writeto(0x23, bytes([0x20])) # set once hi-res mode
time.sleep_ms(180)
data = i2c.readfrom(0x23, 2)
lum = round((data[0]<<8 | data[1]) / 1.2, 3)
print("Luminosity: %0.2f" % lum)

machine.Pin(10, machine.Pin.OUT).off()

# WIFI UPDATE
if wifi_update:
    uploaded = False
    connected = False
    
    sta_if = network.WLAN(network.STA_IF)
    sta_if.active(True)
    sta_if.connect(ssid, password)
    
    for i in range(100):  
        if sta_if.isconnected():
            connected = True
            break                
        else: time.sleep_ms(100)
            
    if connected:
        print("Connected to WiFi")
        readings = {'field1':temp, 'field2':humi, 'field3':lum}
        try:
            request = urequests.post(url, json = readings, headers = {'Content-Type': 'application/json'})  
            request.close()
            print("Data sent")
            uploaded = True
        except Exception as e: print("Data transfer failed")
        sta_if.disconnect()
        
    else: print("Connection failed")
    
    sta_if.active(False)
    
# VALUES LOG
if not "log.csv" in os.listdir():
    with open("log.csv", "a") as log:
        log.write("date,time,temp,hum,lum")
        if wifi_update: log.write(",connected,uploaded")
        log.write("\n")
        
with open("log.csv", "a") as log:
    log.write(rtc_date+","+rtc_time+","+str(temp)+","+str(humi)+","+str(lum))
    if wifi_update: log.write(","+str(connected)+","+str(uploaded))
    log.write("\n")
    
# DEEP SLEEP TIME
now = time.localtime()
sleep_time = log_period - ((now[4]*60 + now[5]) % log_period)
print("Going to sleep for %0.2f minutes" % (sleep_time/60))
if not test: machine.deepsleep(sleep_time*1000)
# end
