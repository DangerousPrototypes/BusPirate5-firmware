from bpio_client import BPIOClient

class BPIOSPI:
    def __init__(self, client):
        self.client = client
        self.configured = False
    
    def configure(self, speed=1000000, clock_polarity=False, clock_phase=False, chip_select_idle=True, **kwargs):
        """Configure SPI mode"""
        success = self.client.configure_mode(
            "SPI", 
            speed=speed,
            clock_polarity=clock_polarity,
            clock_phase=clock_phase,
            chip_select_idle=chip_select_idle,
            **kwargs
        )
        self.configured = success
        return success
    
    def config_check(self):
        """Check if SPI is configured"""
        if not self.configured:
            print("SPI not configured. Call configure() first.")
            return False
        return True
    
    def select(self):
        """Select SPI device (set chip select low)"""
        if not self.config_check():
            return None
            
        return self.client.data_request(
            start_main=True
        )
    
    def deselect(self):
        """Deselect SPI device (set chip select high)"""
        if not self.config_check():
            return None
            
        return self.client.data_request(
            stop_main=True
        )
    
    def write(self, data):
        """Write data to SPI device"""
        if not self.config_check():
            return None
            
        return self.client.data_request(
            data_write=data
        )
    
    def read(self, num_bytes):
        """Read bytes from SPI device"""
        if not self.config_check():
            return None
            
        return self.client.data_request(
            bytes_read=num_bytes
        )
        
    def transfer(self, write_data, read_bytes=None):
        """Perform SPI transfer"""
        if not self.configured:
            print("SPI not configured. Call configure() first.")
            return None
            
        return self.client.data_request(
            start_main=True,
            data_write=write_data,
            bytes_read=read_bytes,
            stop_main=True
        )
