from bpio_client import BPIOClient
from bpio_base import BPIOBase

class BPIOSPI(BPIOBase):
    def __init__(self, client):
        super().__init__(client)
    
    def configure(self, speed=1000000, clock_polarity=False, clock_phase=False, chip_select_idle=True, **kwargs):
        """Configure SPI mode"""
        kwargs['mode'] = 'SPI'
        # Get the existing mode_configuration from kwargs or create a new one
        mode_configuration = kwargs.get('mode_configuration', {})
        # Set the SPI configuration parameters
        mode_configuration['speed'] = speed
        mode_configuration['clock_polarity'] = clock_polarity
        mode_configuration['clock_phase'] = clock_phase
        mode_configuration['chip_select_idle'] = chip_select_idle
        # Replace the mode_configuration in kwargs
        kwargs['mode_configuration'] = mode_configuration

        success = self.client.configuration_request(**kwargs)         
        self.configured = success
        return success
       
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
