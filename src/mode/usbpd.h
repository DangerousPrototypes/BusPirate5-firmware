uint32_t usbpd_setup(void);
uint32_t usbpd_setup_exc(void);
void usbpd_cleanup(void);

void usbpd_help(void);
void usbpd_settings(void);

extern const struct _mode_command_struct usbpd_commands[];
extern const uint32_t usbpd_commands_count;
