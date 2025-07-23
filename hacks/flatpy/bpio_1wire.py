from bpio_client import BPIOClient
import time

class BPIO1Wire:
    def __init__(self, client):
        self.client = client
        self.configured = False
   
    def configure(self, **kwargs):
        """Configure 1-Wire mode"""
        success = self.client.configure_mode("1WIRE", **kwargs)
        self.configured = success
        return success

    def config_check(self):
        """Check if 1-Wire is configured"""
        if not self.configured:
            print("1-Wire not configured. Call configure() first.")
            return False
        return True

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
    