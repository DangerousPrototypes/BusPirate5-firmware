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
          
def show_progress(current, total, start_time, operation_name="Operation", unit="MB"):
    """Reusable progress indicator function."""
    progress = current / total
    bar_length = 50
    filled_length = int(bar_length * progress)
    bar = '█' * filled_length + '░' * (bar_length - filled_length)
    percent = progress * 100
    
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
    
    # Speed calculation
    if unit == "MB":
        current_mb = current / (1024 * 1024)
        total_mb = total / (1024 * 1024)
        speed_kbps = (current_mb * 1024) / elapsed_time if elapsed_time > 0 else 0
        size_info = f"({current_mb:.1f}{unit}/{total_mb:.1f}{unit})"
        speed_info = f"({speed_kbps:.1f}KB/s)"
    else:
        speed_kbps = current / elapsed_time if elapsed_time > 0 else 0
        size_info = f"({current}{unit}/{total}{unit})"
        speed_info = f"({speed_kbps:.1f}/s)"
    
    # Print progress on two lines
    print(f'\r[{bar}] {percent:.1f}%', end='')
    print(f'\n{size_info} {elapsed_str} elapsed, ETA {eta_str} {speed_info}', end='')
    print('\033[A', end='', flush=True)  # Move cursor up one line for next update

# example to read 128mbit SPI flash and save to file
def read_spi_flash_to_file(client, filename='spi_flash_dump.bin'):
    """Read the entire SPI flash and save it to a file."""
    spi = BPIOSPI(client)
    if spi.configure(speed=12*1000*1000, clock_polarity=False, clock_phase=False, chip_select_idle=True, psu_enable=True, psu_voltage_mv=3300, psu_current_ma=0, pullup_enable=True):
        total_size = 16 * 1024 * 1024  # 16MB
        chunk_size = 512
        total_chunks = total_size // chunk_size
        
        print(f"Reading {total_size // (1024*1024)}MB SPI flash to '{filename}'...")
        
        with open(filename, 'wb') as f:
            address = 0
            chunk_count = 0
            start_time = time.time()
            
            while address < total_size:
                # Read chunk from the SPI flash
                data = spi.transfer(write_data=[0x03, (address >> 16) & 0xFF, (address >> 8) & 0xFF, address & 0xFF], read_bytes=chunk_size)
                if data and len(data) == chunk_size:
                    f.write(data)
                    address += chunk_size
                    chunk_count += 1
                    
                    # Update progress bar every 256 chunks (128KB)
                    if chunk_count == 1 or chunk_count % 256 == 0 or chunk_count == total_chunks:
                        show_progress(address, total_size, start_time, "Reading")
                else:
                    print(f"\nFailed to read SPI flash data at address 0x{address:06X}")
                    break
            
            total_time = time.time() - start_time
            total_time_str = f"{int(total_time//60):02d}:{int(total_time%60):02d}"
            print(f"\nCompleted! Saved {chunk_count * chunk_size} bytes to '{filename}' in {total_time_str}")

def write_spi_flash_from_file(client, filename='spi_flash_dump.bin', verify=True):
    """Write a file to SPI flash and optionally verify."""
    import os
    
    spi = BPIOSPI(client)
    if spi.configure(speed=12*1000*1000, clock_polarity=False, clock_phase=False, chip_select_idle=True, psu_enable=True, psu_voltage_mv=3300, psu_current_ma=0, pullup_enable=True):
        
        # Check if file exists
        if not os.path.exists(filename):
            print(f"Error: File '{filename}' not found")
            return False
            
        file_size = os.path.getsize(filename)
        max_flash_size = 16 * 1024 * 1024  # 16MB
        
        if file_size > max_flash_size:
            print(f"Error: File size ({file_size} bytes) exceeds flash capacity ({max_flash_size} bytes)")
            return False
            
        print(f"Writing {file_size // (1024*1024):.1f}MB from '{filename}' to SPI flash...")
        
        # First, erase the flash (chip erase for full erase)
        print("Erasing flash chip...")
        start_time = time.time()
        #spi.transfer(write_data=[0x06])  # Write enable
        #spi.transfer(write_data=[0xC7])  # Chip erase command
        
        # Wait for erase to complete (poll status register)
        print("Waiting for erase to complete...")
        while True:
            status_data = spi.transfer(write_data=[0x05], read_bytes=1)
            if status_data and len(status_data) == 1:
                if (status_data[0] & 0x01) == 0:  # WIP bit cleared
                    break
            time.sleep(0.1)

            # Show elapsed time
            elapsed = time.time() - start_time
            print(f'\rErasing... {elapsed:.1f}s elapsed', end='', flush=True)
        
        total_time = time.time() - start_time
        print(f"\nErase completed in {total_time:.1f}s!")
        
        # Write the file data
        page_size = 256  # Standard page size for SPI flash
        chunk_size = page_size
        total_chunks = (file_size + chunk_size - 1) // chunk_size  # Round up
        
        with open(filename, 'rb') as f:
            address = 0
            chunk_count = 0
            start_time = time.time()
            
            while address < file_size:
                # Read chunk from file
                chunk_data = f.read(chunk_size)
                if not chunk_data:
                    break
                    
                # Write enable before each page program
                spi.transfer(write_data=[0x06])
                
                # Page program command (0x02) + 24-bit address + data
                write_cmd = [0x02, (address >> 16) & 0xFF, (address >> 8) & 0xFF, address & 0xFF]
                write_cmd.extend(chunk_data)
                spi.transfer(write_data=write_cmd)
                
                # Wait for write to complete
                while True:
                    #time.sleep(0.002)
                    status_data = spi.transfer(write_data=[0x05], read_bytes=1)
                    if status_data and len(status_data) == 1:
                        if (status_data[0] & 0x01) == 0:  # WIP bit cleared
                            break
                    time.sleep(0.001)
                
                address += len(chunk_data)
                chunk_count += 1
                
                # Update progress bar every 64 chunks (16KB)
                if chunk_count == 1 or chunk_count % 64 == 0 or chunk_count == total_chunks:
                    show_progress(address, file_size, start_time, "Writing")
            
            total_time = time.time() - start_time
            total_time_str = f"{int(total_time//60):02d}:{int(total_time%60):02d}"
            print(f"\nWrite completed! Wrote {address} bytes to flash in {total_time_str}")
            
            # Verify if requested
            if verify:
                print("Verifying written data...")
                verify_start = time.time()
                
                # Reset file pointer
                f.seek(0)
                address = 0
                verify_chunk_size = 512  # Larger chunks for faster verification
                
                while address < file_size:
                    # Read expected data from file
                    expected_data = f.read(verify_chunk_size)
                    if not expected_data:
                        break
                    
                    # Read actual data from flash
                    read_cmd = [0x03, (address >> 16) & 0xFF, (address >> 8) & 0xFF, address & 0xFF]
                    actual_data = spi.transfer(write_data=read_cmd, read_bytes=len(expected_data))
                    
                    if actual_data != expected_data:
                        print(f"\nVerification failed at address 0x{address:06X}")
                        return False
                    
                    address += len(expected_data)
                    
                    # Show verification progress every 64KB
                    if address % (64 * 1024) == 0 or address >= file_size:
                        show_progress(address, file_size, verify_start, "Verifying")
                
                verify_time = time.time() - verify_start
                print(f"\nVerification completed successfully in {verify_time:.1f}s!")
                
        return True
    
    else:
        print("Failed to configure SPI interface")
        return False

def erase_spi_flash(client):
    """Erase the entire SPI flash chip."""
    spi = BPIOSPI(client)
    if spi.configure(speed=12*1000*1000, clock_polarity=False, clock_phase=False, chip_select_idle=True, psu_enable=True, psu_voltage_mv=3300, psu_current_ma=0, pullup_enable=True):
        
        print("Erasing SPI flash chip...")
        start_time = time.time()
        
        # Write enable
        spi.transfer(write_data=[0x06])
        
        # Chip erase command
        spi.transfer(write_data=[0xC7])
        
        # Wait for erase to complete (poll status register)
        print("Waiting for erase to complete (this may take several minutes)...")
        while True:
            status_data = spi.transfer(write_data=[0x05], read_bytes=1)
            if status_data and len(status_data) == 1:
                if (status_data[0] & 0x01) == 0:  # WIP bit cleared
                    break
            time.sleep(0.5)  # Check every 500ms
            
            # Show elapsed time
            elapsed = time.time() - start_time
            print(f'\rErasing... {elapsed:.1f}s elapsed', end='', flush=True)
        
        total_time = time.time() - start_time
        print(f"\nErase completed in {total_time:.1f}s!")
        return True
    else:
        print("Failed to configure SPI interface")
        return False     

# Create client, configure the serial port
client = BPIOClient('COM35')

# Show all available status information
client.show_status()

# Time the operation
start_time = time.time()

# Run the I2C example
#i2c_example(client)

#i2c_example_read_24x02_eeprom(client)

# Run the SPI example
#spi_example(client)

# Run the 1-Wire example
one_wire_example(client)

# Read SPI flash to file
#read_spi_flash_to_file(client, 'spi_flash_dump.bin')

# Erase flash chip
#erase_spi_flash(client)

# Write a file to flash with verification
# write_spi_flash_from_file(client, 'spi_flash_dump.bin', verify=True)

# Write without verification (faster)
# write_spi_flash_from_file(client, 'data.bin', verify=False)

# Read flash to verify
# read_spi_flash_to_file(client, 'verify_dump.bin')

# Print the time taken foe the operation
end_time = time.time()
print(f"Completed in {end_time - start_time:.2f} seconds")

# Close the client connection
client.close()








