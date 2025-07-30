from bpio_client import BPIOClient
from bpio_i2c import BPIOI2C
from bpio_spi import BPIOSPI
from bpio_1wire import BPIO1Wire
import time

# I2C Example
def i2c_example(client):
    """Example of using the I2C interface."""
    # Create a BPIOI2C instance
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

        if not i2c.set_print_string("Hello, from the BPIO2 interface!\r\n"):
            print("Failed to set print string.")
        
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

def i2c_example_read_24x02_eeprom(client):
    """Example of reading from a 24x02 EEPROM using I2C."""
    # Create a BPIOI2C instance
    i2c = BPIOI2C(client)
    if i2c.configure(speed=400000, pullup_enable=True, psu_enable=True, psu_voltage_mv=3300, psu_current_ma=0):
        # Read 24x02 EEPROM
        data = i2c.transfer(write_data=[0xA0, 0x00], read_bytes=256)
        if data:
            print(f"Read data: {data.hex()}")
        else:
            print("Failed to read data.")

# SPI Example 
def spi_example(client):
    spi = BPIOSPI(client)
    if spi.configure(speed=100000, clock_polarity=False, clock_phase=False, chip_select_idle=True, psu_enable=True, psu_voltage_mv=3300, psu_current_ma=0, pullup_enable=True):
        # read SPI flash JEDEC ID
        data = spi.transfer(write_data=[0x9F], read_bytes=3)
        if data and len(data) == 3:
            print(f"SPI Flash ID: {data.hex()}")
        else:
            print("Failed to read SPI Flash ID.")

# 1-Wire Example
def one_wire_example(client):
    """Example of using the 1-Wire interface to read temperature from a DS18B sensor."""
    # Create a BPIO1Wire instance
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
          


# example to read 128mbit SPI flash and save to file
def read_spi_flash_to_file(client, filename='spi_flash_dump.bin'):
    """Read the entire SPI flash and save it to a file."""
    spi = BPIOSPI(client)
    if spi.configure(speed=12*1000*1000, clock_polarity=False, clock_phase=False, chip_select_idle=True, psu_enable=True, psu_voltage_mv=3300, psu_current_ma=0, pullup_enable=True):
        total_size = 16 * 1024 * 1024  # 16MB
        chunk_size = 256
        total_chunks = total_size // chunk_size
        
        print(f"Reading {total_size // (1024*1024)}MB SPI flash to '{filename}'...")
        
        # Read the entire flash memory in 256 byte chunks
        with open(filename, 'wb') as f:
            address = 0
            chunk_count = 0
            start_time = time.time()  # Start timing
            
            while address < total_size:
                # Read 256 bytes from the SPI flash
                data = spi.transfer(write_data=[0x03, (address >> 16) & 0xFF, (address >> 8) & 0xFF, address & 0xFF], read_bytes=chunk_size)
                if data and len(data) == chunk_size:
                    f.write(data)
                    address += chunk_size
                    chunk_count += 1
                    # Update progress bar every 256 chunks (64KB)
                    if chunk_count == 1 or chunk_count % 256 == 0 or chunk_count == total_chunks:
                        progress = chunk_count / total_chunks
                        bar_length = 50
                        filled_length = int(bar_length * progress)
                        bar = '█' * filled_length + '░' * (bar_length - filled_length)
                        percent = progress * 100
                        mb_read = (chunk_count * chunk_size) / (1024 * 1024)
                        
                        # Calculate elapsed time and estimated time remaining
                        elapsed_time = time.time() - start_time
                        if progress > 0:
                            estimated_total_time = elapsed_time / progress
                            eta = estimated_total_time - elapsed_time
                        else:
                            eta = 0
                        
                        # Format time as mm:ss
                        elapsed_str = f"{int(elapsed_time//60):02d}:{int(elapsed_time%60):02d}"
                        eta_str = f"{int(eta//60):02d}:{int(eta%60):02d}"
                        
                        # For more detailed timing with speed calculation
                        speed_kbps = (mb_read * 1024) / elapsed_time if elapsed_time > 0 else 0
                        
                        # Print the progress bar and status on two lines
                        print(f'\r[{bar}] {percent:.1f}%', end='')
                        print(f'\n({mb_read:.1f}MB/{total_size//(1024*1024)}MB) {elapsed_str} elapsed, ETA {eta_str} ({speed_kbps:.1f}KB/s)', end='')
                        print('\033[A', end='', flush=True)  # Move cursor up one line for next update                   
                else:
                    print(f"\nFailed to read SPI flash data at address 0x{address:06X}")
                    break
            
            total_time = time.time() - start_time
            total_time_str = f"{int(total_time//60):02d}:{int(total_time%60):02d}"
            print(f"\nCompleted! Saved {chunk_count * chunk_size} bytes to '{filename}' in {total_time_str}")

# Create client, configure the serial port
client = BPIOClient('COM35')

# Show all available status information
client.show_status()

# Time the operation
start_time = time.time()

# Run the I2C example
#i2c_example(client)

# Run the SPI example
#spi_example(client)

# Run the 1-Wire example
#one_wire_example(client)

# Read SPI flash to file
read_spi_flash_to_file(client, 'spi_flash_dump.bin')

# Print the time taken foe the operation
end_time = time.time()
print(f"Completed in {end_time - start_time:.2f} seconds")

# Close the client connection
client.close()








