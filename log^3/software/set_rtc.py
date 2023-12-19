import machine, time

now = time.localtime()
print('localtime :', now)

machine.RTC().datetime((now[0], now[1], now[2], 1, now[3], now[4], now[5], 1))
print('machine RTC set !')

i2c = machine.SoftI2C(sda = machine.Pin(6), scl = machine.Pin(7))

# set each time unit in PCF8563
i2c.writeto_mem(0x51, 0x02, bytes([(divmod(now[5], 10)[0] << 4) + divmod(now[5], 10)[1]]))
i2c.writeto_mem(0x51, 0x03, bytes([(divmod(now[4], 10)[0] << 4) + divmod(now[4], 10)[1]]))
i2c.writeto_mem(0x51, 0x04, bytes([(divmod(now[3], 10)[0] << 4) + divmod(now[3], 10)[1]]))
i2c.writeto_mem(0x51, 0x05, bytes([(divmod(now[2], 10)[0] << 4) + divmod(now[2], 10)[1]]))
i2c.writeto_mem(0x51, 0x06, b'\x01')
i2c.writeto_mem(0x51, 0x07, bytes([(divmod(now[1], 10)[0] << 4) + divmod(now[1], 10)[1]]))
i2c.writeto_mem(0x51, 0x08, bytes([(divmod(now[0]%100, 10)[0] << 4) + divmod(now[0]%100, 10)[1]]))
print('external RTC set !')
