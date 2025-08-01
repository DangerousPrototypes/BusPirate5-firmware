#from bpio_client import BPIOClient

class BPIOBase:  
    def __init__(self, client):
        self.client = client
        self.configured = False

    def config_check(self):
        """Check if configured"""
        if not self.configured:
            print("Not connected. Call configure() first.")
            return False
        return True    
    
    def configuration_request(self, **kwargs):
        """Pass configuration parameters to the client"""
        if not self.config_check():
            return None       
        return self.client.configuration_request(**kwargs)    
    
    # Functions to set each configuration parameter
    def set_mode_bitorder_msb(self):
        """Set mode bit order to MSB first"""
        if not self.config_check():
            return None
        return self.client.configuration_request(
            mode_bitorder_msb=True
        )
    def set_mode_bitorder_lsb(self):
        """Set mode bit order to LSB first"""
        if not self.config_check():
            return None
        return self.client.configuration_request(
            mode_bitorder_lsb=True
        )
    def set_psu_disable(self):
        """Disable power supply"""
        if not self.config_check():
            return None
        return self.client.configuration_request(
            psu_disable=True
        )
    def set_psu_enable(self, voltage_mv=3300, current_ma=300):
        """Enable power supply with specified voltage and current"""
        if not self.config_check():
            return None
        return self.client.configuration_request(
            psu_enable=True,
            psu_set_mv=voltage_mv,
            psu_set_ma=current_ma
        )
    def set_pullup_disable(self):
        """Disable pull-up resistors"""
        if not self.config_check():
            return None
        return self.client.configuration_request(
            pullup_disable=True
        )
    def set_pullup_enable(self):
        """Enable pull-up resistors"""
        if not self.config_check():
            return None
        return self.client.configuration_request(
            pullup_enable=True
        )
    def set_pullx_config(self, config): 
        """Set pull-x configuration for BP7+"""
        if not self.config_check():
            return None
        return self.client.configuration_request(
            pullx_config=config
        )
    def set_io_direction(self, direction_mask, direction):
        """Set IO pin directions"""
        if not self.config_check():
            return None
        return self.client.configuration_request(
            io_direction_mask=direction_mask,
            io_direction=direction
        )
    def set_io_value(self, value_mask, value):
        """Set IO pin values"""
        if not self.config_check():
            return None
        return self.client.configuration_request(
            io_value_mask=value_mask,
            io_value=value
        )
    def set_led_resume(self):
        """Resume LED effect after configuration"""
        if not self.config_check():
            return None
        return self.client.configuration_request(
            led_resume=True
        )
    def set_led_color(self, colors):
        """Set LED colors"""
        if not self.config_check():
            return None
        return self.client.configuration_request(
            led_color=colors
        )
    def set_print_string(self, string):
        """Print string on terminal"""
        if not self.config_check():
            return None
        return self.client.configuration_request(
            print_string=string
        )
    def set_hardware_bootloader(self):
        """Enter bootloader mode"""
        if not self.config_check():
            return None
        return self.client.configuration_request(
            hardware_bootloader=True
        )
    def set_hardware_reset(self):
        """Hardware reset the device"""
        if not self.config_check():
            return None
        return self.client.configuration_request(
            hardware_reset=True
        )
    # Functions to get each status parameter
    def show_status(self):
        """Get and print status information"""
        if not self.config_check():
            return None
        status_dict = self.client.status_request()
        if status_dict:
            self.print_status_response(status_dict)
        else:
            print("Failed to get status information.")
    def get_status(self):
        """Get status information"""
        if not self.config_check():
            return None
        return self.client.status_request()
    
    def get_hardware_version_major(self):
        """Get hardware version major"""
        if not self.config_check():
            return None
        status_dict = self.client.status_request(
            version=True
        )
        return status_dict.get('hardware_version_major', None)
    def get_hardware_version_minor(self):
        """Get hardware version minor"""
        if not self.config_check():
            return None
        status_dict = self.client.status_request(
            version=True
        )
        return status_dict.get('hardware_version_minor', None)
    
    def get_firmware_version_major(self):
        """Get firmware version major"""
        if not self.config_check():
            return None
        status_dict = self.client.status_request(
            version=True
        )
        return status_dict.get('firmware_version_major', None)
    def get_firmware_version_minor(self):
        """Get firmware version minor"""
        if not self.config_check(): 
            return None
        status_dict = self.client.status_request(
            version=True
        )
        return status_dict.get('firmware_version_minor', None)
    def get_firmware_git_hash(self):
        """Get firmware git hash"""
        if not self.config_check():
            return None
        status_dict = self.client.status_request(
            version=True
        )
        return status_dict.get('firmware_git_hash', None)
    def get_firmware_date(self):
        """Get firmware date"""
        if not self.config_check():
            return None
        status_dict = self.client.status_request(
            version=True
        )
        return status_dict.get('firmware_date', None)
    def get_modes_available(self):
        """Get available modes"""
        if not self.config_check():
            return None
        status_dict = self.client.status_request(
            mode=True
        )
        return status_dict.get('modes_available', None)
    def get_mode_current(self):
        """Get current mode"""
        if not self.config_check():
            return None
        status_dict = self.client.status_request(
            mode=True
        )
        return status_dict.get('mode_current', None)
    def get_mode_pin_labels(self):
        """Get mode pin labels"""
        if not self.config_check():
            return None
        status_dict = self.client.status_request(
            mode=True
        )
        return status_dict.get('mode_pin_labels', None)
    def get_mode_bitorder_msb(self):
        """Get mode bit order MSB"""
        if not self.config_check():
            return None
        status_dict = self.client.status_request(
            mode=True
        )
        return status_dict.get('mode_bitorder_msb', False)
    def get_psu_enabled(self):
        """Get power supply enabled status"""
        if not self.config_check():
            return None
        status_dict = self.client.status_request(
            psu=True
        )
        return status_dict.get('psu_enabled', False)
    def get_psu_set_mv(self):
        """Get power supply set voltage in mV"""
        if not self.config_check():
            return None
        status_dict = self.client.status_request(
            psu=True
        )
        return status_dict.get('psu_set_mv', None)
    def get_psu_set_ma(self):
        """Get power supply set current in mA"""
        if not self.config_check():
            return None
        status_dict = self.client.status_request(
            psu=True
        )
        return status_dict.get('psu_set_ma', None)
    def get_psu_measured_mv(self):
        """Get power supply measured voltage in mV"""
        if not self.config_check():
            return None
        status_dict = self.client.status_request(           
            psu=True
        )
        return status_dict.get('psu_measured_mv', None)
    def get_psu_measured_ma(self):
        """Get power supply measured current in mA"""
        if not self.config_check():
            return None
        status_dict = self.client.status_request(
            psu=True
        )
        return status_dict.get('psu_measured_ma', None)
    def get_psu_current_error(self):
        """Get power supply current error"""
        if not self.config_check():
            return None
        status_dict = self.client.status_request(
            psu=True
        )
        return status_dict.get('psu_current_error', False)
    def get_pullup_enabled(self):
        """Get pull-up resistors enabled status"""
        if not self.config_check():
            return None
        status_dict = self.client.status_request(
            pullup=True
        )
        return status_dict.get('pullup_enabled', False)
    def get_pullx_config(self):
        """Get pull-x configuration for BP7+"""
        if not self.config_check():
            return None
        status_dict = self.client.status_request(
            pullx=True
        )
        return status_dict.get('pullx_config',None)
    def get_adc_mv(self):
        """Get ADC values in mV"""
        if not self.config_check():
            return None
        status_dict = self.client.status_request(
            adc=True
        )
        return status_dict.get('adc_mv', None)
    def get_io_direction(self):
        """Get IO pin directions"""
        if not self.config_check():
            return None
        status_dict = self.client.status_request(
            io=True
        )
        return status_dict.get('io_direction', None)
    def get_io_value(self):
        """Get IO pin values"""
        if not self.config_check():
            return None
        status_dict = self.client.status_request(
            io=True
        )
        return status_dict.get('io_value', None)
    def get_disk_size_mb(self):
        """Get disk size in MB"""
        if not self.config_check():
            return None
        status_dict = self.client.status_request(
            disk=True
        )
        return status_dict.get('disk_size_mb', None)
    def get_disk_used_mb(self):
        """Get disk used space in MB"""
        if not self.config_check():
            return None
        status_dict = self.client.status_request(
            disk=True
        )
        return status_dict.get('disk_used_mb', None)
    def get_led_count(self):
        """Get number of LEDs"""
        if not self.config_check():
            return None
        status_dict = self.client.status_request(
            led=True
        )
        return status_dict.get('led_count', None) 
