from bpio_client import BPIOClient
from bpio_base import BPIOBase

class BPIO1Wire(BPIOBase):
    def __init__(self, client):
        super().__init__(client)
   
    def configure(self, **kwargs):
        """Configure 1-Wire mode"""
        kwargs['mode'] = '1Wire'
        #get the existing mode_configuration from kwargs or create a new one
        mode_configuration = kwargs.get('mode_configuration', {})
        # Replace the mode_configuration in kwargs
        kwargs['mode_configuration'] = mode_configuration        
        success = self.client.configuration_request(**kwargs)  
        self.configured = success
        return success

    def reset(self):
        """Reset the 1-Wire bus"""
        if not self.config_check():
            return None
            
        return self.client.data_request(
            start_main=True,
        )
    
    def write(self, data):
        if not self.config_check():
            return None
            
        return self.client.data_request(
            data_write=data,
        )
    
    def read(self, num_bytes):
        """Read bytes from 1-Wire device"""
        if not self.config_check():
            return None
            
        return self.client.data_request(
            bytes_read=num_bytes,
        )

    def transfer(self, write_data=None, read_bytes=0):
        """Perform a 1-Wire transaction"""
        if not self.config_check():
            return None
            
        return self.client.data_request(
            start_main=True,
            data_write=write_data,
            bytes_read=read_bytes,
        )
    