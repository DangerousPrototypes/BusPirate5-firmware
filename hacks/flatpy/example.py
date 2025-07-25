from bpio_client import BPIOClient
from bpio_i2c import BPIOI2C
from bpio_spi import BPIOSPI
from bpio_1wire import BPIO1Wire
import time

# Create client
client = BPIOClient('COM35')

# Get status
client.show_status()

# 1-Wire Example
if False:
    onewire = BPIO1Wire(client)
    if onewire.configure(pullup_enable=True, psu_enable=True, psu_voltage_mv=5000, psu_current_ma=0):
        # Read temperature from DS18B20
        onewire.transfer(write_data=[0xCC, 0x4E, 0x00, 0x00, 0x7F])  # Write scratchpad
        onewire.transfer(write_data=[0xCC, 0x44])  # Start conversion
        time.sleep(0.8)  # Wait for conversion
        data = onewire.transfer(write_data=[0xCC, 0xBE], read_bytes=9)  # Read scratchpad
        if data and len(data) >= 2:
            # Convert temperature (first two bytes)
            temp_raw = (data[1] << 8) | data[0]
            if temp_raw & 0x8000:  # negative temperature
                temp_raw = -((temp_raw ^ 0xFFFF) + 1)
            temperature = temp_raw / 16.0
            print(f"Temperature: {temperature:.2f}°C")
        else:
            print("Failed to read temperature data.")
        quit()

# I2C Example
if True:
    i2c = BPIOI2C(client)
    # Configure the mode, other configurations can be set such as pull-up resistors and power supply
    if i2c.configure(speed=400000, pullup_enable=True, psu_enable=True, psu_voltage_mv=3300, psu_current_ma=0):
        # Individual configuration settings are possible, but are less efficient
        if not i2c.set_psu_enable(voltage_mv=3300, current_ma=0):
            print("Failed to enable PSU.")
        if not i2c.set_pullup_enable():
            print("Failed to enable pull-up resistors.")
        if not i2c.set_mode_bitorder_msb():
            print("Failed to set MSB first bit order.")
        # set the LED color (array of 18 RGB values)
        # Create LED colors (RGB format)
        # An array of 18 RGB LED colors, each represented as a 32-bit integer (0xRRGGBB)
        led_colors = [0xFF0000, 0x00FF00, 0x0000FF, 0xFFFF00, 0xFF00FF, 0x00FFFF] * 3
        if not i2c.set_led_color(led_colors):
            print("Failed to set LED color.")
        
        # get some status information
        print("Current mode:", i2c.get_mode_current())
        print("PSU enabled:", i2c.get_psu_enabled())
        print("PSU voltage (mV):", i2c.get_psu_measured_mv())
        print("PSU current (mA):", i2c.get_psu_measured_ma())
        print("Pull-up enabled:", i2c.get_pullup_enabled())

        # read 24x02 EEPROM    
        data = i2c.transfer(write_data=[0xA0, 0x00], read_bytes=8)
        if data:
            print(f"Read data: {data.hex()}")
        else:
            print("Failed to read data.")

# SPI Example 
if False:
    spi = BPIOSPI(client)
    if spi.configure(speed=100000, clock_polarity=False, clock_phase=False, chip_select_idle=True, psu_enable=True, psu_voltage_mv=3300, psu_current_ma=0, pullup_enable=True):
        # read SPI flash JEDEC ID
        data = spi.transfer(write_data=[0x9F], read_bytes=3)
        if data and len(data) == 3:
            print(f"SPI Flash ID: {data.hex()}")
        else:
            print("Failed to read SPI Flash ID.")


