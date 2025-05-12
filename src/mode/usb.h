uint32_t usb_setup(void);
uint32_t usb_setup_exc(void);
void usb_cleanup(void);
bool usb_preflight_sanity_check(void);
void usb_pins(void);
void usb_settings(void);
void usb_help(void);
uint32_t usb_get_speed(void);


extern const struct _mode_command_struct usb_commands[];
extern const uint32_t usb_commands_count;
