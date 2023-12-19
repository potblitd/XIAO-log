# log<sup>2

Adding accurate time keeping and a battery voltage reading to the XIAO ESP32C3 with this weird shaped PCB. Here the PCF8563 RTC chip is powered continuously through the 3V3, until battery dies I guess. Battery voltage divider is enabled through D10 and the halved voltage can be read on A3. Footprints and silkscreen are designed to help for manual soldering.

## Assembly

Intended to be soldered directly onto the XIAO board (screw heat dissipation), connecting the BAT+ pins together while leaving space for a connector. 

## Code

*set_rtc.py* can be used to set the time on PCF8563 form the local time on the PC.

*xiaortc.py* first inits I2C comunication, updates the time from the RTC chip, reads temperature and humidity from the SHT40 sensor, writes all these values in a file and then calculates the time until the next measurement based on the log period before going into deep sleep.

Use the *tes* variable to avoid the logger going to deep sleep and loosing connection when testing the code. Also, to acess files after a run, connecting or resetting the board gives the user 10 seconds to do so.





