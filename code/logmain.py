# PARAMETERS
log_period = 60

read_battery = True

wifi_update = True
ssid = 'EcoNet'
password = 'Ecoeco12345678'
url = 'http://api.thingspeak.com/update?api_key='
api_key = '9QHAF1Z4VPE9P2WA'


# LIBRAIRIES
import os, time, machine
if wifi_update: import network, urequests

# DATE & TIME
now = time.localtime()
now_date = "-".join(map(str, now[0:3]))
now_time = ":".join(map(str, now[3:6]))
print("Date and time:", now_date, now_time)

# BATTERY VOLTAGE
if read_battery:
    # add code here
    vbat = 0

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
temp = -45 + 175 * (buf[0]<<8 | buf[1]) / 65535
humi = -6 + 125 * (buf[3]<<8 | buf[4]) / 65535
print("Temperature: %0.1f C" % temp)
print("Humidity: %0.1f %%" % humi)
    
# WIFI UPDATE
if wifi_update:
    sta_if = network.WLAN(network.STA_IF)
    sta_if.active(True)
    
    if not sta_if.isconnected():
        print('connecting to network...')
        sta_if.connect(ssid, password)
        while not sta_if.isconnected():
            pass
    print('network config:', sta_if.ifconfig())
    
    if read_battery: readings = {'field1':temp, 'field2':humi, 'field3':vbat}
    else: readings = {'field1':temp, 'field2':humi}
    try:
        request = urequests.post(url + api_key, json = readings, headers = {'Content-Type': 'application/json'})  
        request.close()
        print("Data sucessfully sent")
        upload = "success"
    except Exception as e:
        print("Data send failed")
        print(e)
        upload = "failed"
        
# VALUES LOG
if not "log.csv" in os.listdir():
    with open("log.csv", "a") as log:
        log.write("date,time,temp,hum")
        if read_battery: log.write(",vbat")
        if wifi_update:log.write(",upload")
        log.write("\n")
with open("log.csv", "a") as log:
    log.write(now_date+","+now_time+","+str(temp)+","+str(humi))
    if read_battery: log.write(","+str(vbat))
    if wifi_update:log.write(","+str(upload))
    log.write("\n")    
    
# DEEP SLEEP TIME
now_second = now[4]*60 + now[5]
sleep_time = log_period - (now_second % log_period)
print("Going to sleep for %0.2f minutes" % (sleep_time/60))
machine.deepsleep(sleep_time*1000)
# end

