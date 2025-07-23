from bpio_client import BPIOClient

class BPIOI2C:
    def __init__(self, client):
        self.client = client
        self.configured = False
    
    def configure(self, speed=400000, clock_stretch=False, **kwargs):
        """Configure I2C mode"""
        success = self.client.configure_mode("I2C", speed=speed, clock_stretch=clock_stretch, **kwargs)
        self.configured = success
        return success
    
    def config_check(self):
        """Check if I2C is configured"""
        if not self.configured:
            print("I2C not configured. Call configure() first.")
            return False
        return True
    
    def start(self):
        """I2C Start condition"""
        if not self.config_check():
            return None
            
        return self.client.data_request(
            start_main=True,
        )
    
    def stop(self):
        """I2C Stop condition"""
        if not self.config_check():
            return None
            
        return self.client.data_request(
            stop_main=True,
        )
    
    def write(self, data):
        """Write data to I2C device"""
        if not self.config_check():
            return None
            
        return self.client.data_request(
            data_write=data,
        )
    
    def read(self, num_bytes):
        """Read bytes from I2C device"""
        if not self.config_check():
            return None
            
        return self.client.data_request(
            bytes_read=num_bytes,
        )   
    
    def transfer(self, write_data=None, read_bytes=0):
        """Perform an I2C transaction"""
        if not self.config_check():
            return None
            
        return self.client.data_request(
            start_main=True,
            data_write=write_data,
            bytes_read=read_bytes,
            stop_main=True
        )
    
    def scan(self, start_addr=0x00, end_addr=0x7F):
        """Scan for I2C devices"""
        if not self.config_check():
            return []
            
        found_devices = []
        print(f"Scanning I2C bus from 0x{start_addr:02X} to 0x{end_addr:02X}...")
        
        for addr in range(start_addr, end_addr):
            #for write addresses, start, address, stop
            result = self.transfer(write_data=[addr << 1], read_bytes=0)
            if result is not None:
                found_devices.append(addr<<1)
                #print(f"Device found at 0x{addr:02X}")
            #else:
                #self.stop() #clear bus if write failed

            #for read addresses, start, address + 1, read 1 byte to NACK, stop
            result = self.transfer(write_data=[(addr << 1) | 1], read_bytes=1)
            if result is not None:
                found_devices.append(addr<<1|1)
                #print(f"Device found at 0x{addr:02X} (read)")
            #else:
                #self.stop() #clear bus if read failed
        self.stop()  # Ensure we stop the bus after scanning
                
        return found_devices