from bpio_client import BPIOClient
from bpio_i2c import BPIOI2C
from bpio_spi import BPIOSPI
from bpio_1wire import BPIO1Wire
import time

# Create client
client = BPIOClient('COM35')

# Get status
client.status_request()

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
if False:
    i2c = BPIOI2C(client)
    if i2c.configure(speed=400000, pullup_enable=True, psu_enable=True, psu_voltage_mv=3300, psu_current_ma=0):
        #scan for devices
        devices = i2c.scan()
        if devices:
            print(f"Found I2C devices: {', '.join(f'0x{addr:02X}' for addr in devices)}")
        else:
            print("No I2C devices found.")
        # read 24x02 EEPROM    
        data = i2c.transfer(write_data=[0xA0, 0x00], read_bytes=8)
        if data:
            print(f"Read data: {data.hex()}")
        else:
            print("Failed to read data.")

# SPI Example 
if True:
    spi = BPIOSPI(client)
    if spi.configure(speed=100000, clock_polarity=False, clock_phase=False, chip_select_idle=True, psu_enable=True, psu_voltage_mv=3300, psu_current_ma=0, pullup_enable=True):
        # read SPI flash JEDEC ID
        data = spi.transfer(write_data=[0x9F], read_bytes=3)
        if data and len(data) == 3:
            print(f"SPI Flash ID: {data.hex()}")
        else:
            print("Failed to read SPI Flash ID.")


