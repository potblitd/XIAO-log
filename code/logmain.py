# PARAMETERS
log_period = 60*30
test = True
read_battery = True
wifi_update = True
ssid = ''
password = ''
url = 'https://api.thingspeak.com/update?api_key='

# LIBRAIRIES
import os, time, machine, sys
if wifi_update: import network, urequests

# CONNECTION ON RESET
if machine.reset_cause() == 1: time.sleep_ms(10000)

# SENSORS POWER ON
machine.Pin(10, machine.Pin.OUT).on()

# BATTERY VOLTAGE
if read_battery:
    adc = machine.ADC(machine.Pin(5), atten = machine.ADC.ATTN_11DB)
    time.sleep_ms(100)
    vbat = round(adc.read_uv()/1000/1000*2, 3)
    if test : print("Battery voltage: %0.3f V" % vbat)
else: vbat = 'NA'

# I2C INIT
i2c = machine.SoftI2C(sda = machine.Pin(6), scl = machine.Pin(7))
i2c_devices = i2c.scan()
if test : print('I2C adresses currently on the lines :',i2c_devices)

# PCF8563 - DATE & TIME
if 81 in i2c_devices:
    rtc_masks = [0x7F,0x7F,0x3F,0x3F,0x07,0x1F,0xFF]
    rtc = [None]*7
    for i in range(0,7):
        bcd = i2c.readfrom_mem(0x51, i+2, 1)[0] & rtc_masks[i]
        rtc[i] = (((bcd & 0xf0) >> 4) * 10 + (bcd & 0x0f))
        
    rtc_date = str(rtc[6]+2000)+"-"+str(rtc[5])+"-"+str(rtc[3])
    rtc_time = str(rtc[2])+":"+str(rtc[1])+":"+str(rtc[0])
    if test : print("Date and time:", rtc_date, rtc_time)
else:
    rtc_date = 'NA'
    rtc_time = 'NA'

# SHT40 - TEMPERATURE & HUMIDITY
if 68 in i2c_devices:
    i2c.writeto(0x44, bytes([0xFD])) # set hi-res mode
    time.sleep_ms(10)
    buf = i2c.readfrom(0x44, 6)
    temp = round(-45 + 175 * (buf[0]<<8 | buf[1]) / 65535, 3)
    humi = round(-6 + 125 * (buf[3]<<8 | buf[4]) / 65535, 3)
    if test :
        print("Temperature: %0.3f C" % temp)
        print("Humidity: %0.3f %%" % humi)
else:
    temp = 'NA'
    humi = 'NA'    

# BH1750 - LUMINOSITY
if 35 in i2c_devices:
    time.sleep_ms(1)
    i2c.writeto(0x23, bytes([0x01])) # power on
    i2c.writeto(0x23, bytes([0x07])) # reset
    i2c.writeto(0x23, bytes([0x20])) # set once hi-res mode
    time.sleep_ms(180)
    data = i2c.readfrom(0x23, 2)
    lum = round((data[0]<<8 | data[1]) / 1.2, 3)
    if test : print("Luminosity: %0.3f" % lum)
else: lum = 'NA'
    
# SENSORS POWER OFF
machine.Pin(10, machine.Pin.OUT).off()

# WIFI UPDATE
if wifi_update:
    uploaded = False
    connected = False
    sta_if = network.WLAN(network.STA_IF)
    sta_if.active(True)
    sta_if.connect(ssid, password)
    # try to connect
    for i in range(100):  
        if sta_if.isconnected():
            connected = True
            break                
        else: time.sleep_ms(100)
    # try to send data
    if connected:
        print("Connected to WiFi")
        readings = {'field1':temp, 'field2':humi, 'field3':lum, 'field4':vbat}
        try:
            request = urequests.post(url, json = readings, headers = {'Content-Type': 'application/json'})  
            request.close()
            print("Data sent")
            uploaded = True
        except Exception as e: print("Data transfer failed")
        sta_if.disconnect()
    else: print("Connection failed")
    # disconnect
    sta_if.active(False)
    
# VALUES LOG
if not "log.csv" in os.listdir():
    with open("log.csv", "a") as log:
        log.write("date,time,vbat,temp,hum,lum")
        if wifi_update: log.write(",connected,uploaded")
        log.write("\n")
        
with open("log.csv", "a") as log:
    log.write(rtc_date+","+rtc_time+","+str(vbat)+","+str(temp)+","+str(humi)+","+str(lum))
    if wifi_update: log.write(","+str(connected)+","+str(uploaded))
    log.write("\n")
    
# DEEP SLEEP TIME
if not test:
    sleep_time = log_period - ((rtc[1]*60 + rtc[0]) % log_period)
    if sleep_time < 30: sleep_time = sleep_time + log_period
    machine.deepsleep(sleep_time*1000)
# end

