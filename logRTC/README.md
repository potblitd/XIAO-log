# logRTC

Adding accurate time keeping and a battery voltage reading to the XIAO ESP32C3 with this weird shaped PCB. Here the PCF8563 RTC chip is powered continuously through the 3V3. The battery voltage divider is enabled through D10 and the halved voltage can be read on A3. Footprints and silkscreen are specifically designed to help for manual soldering.

<p align="center">
  <img src="images/front-assembled.png" width="200" />
  <img src="images/back.png" width="200" />
</p>

## Assembly

After mounting the SMD components, the module is intended to be soldered directly onto the XIAO board (screw heat dissipation), connecting the BAT+ pins together while leaving space for a connector or wires. 

## Code

*set_rtc.py* can be used to set the time on PCF8563 form the local time on the PC.

The *logmain.py* can be used for this board. The *read_battery* parameter is used to enable battery voltage reading through D10 and A3. I2C comunication is initialized and, according to the adresses on the bus, values the real-time clock (PCF8563), temperature & humidty sensor (SHT40) and ambient light sensor (BH1750) are retrieved. This data is then written into a file and send over WiFi to a cloud if *wifi_update* is enabled and the corresponding parameters are set up. Finally, the time until the next measurement is calculated based on the *log_period* and deep sleep is activated.

Use the *test* variable to avoid the logger going to deep sleep and loosing connection when testing the code. Also, to acess files after a run, resetting the board gives the user 10 seconds to connect to an IDE.





