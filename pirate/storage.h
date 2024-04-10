void storage_init(void);
bool storage_detect(void);
uint8_t storage_mount(void);
uint8_t storage_format(void);
void storage_file_error(uint8_t res);
bool storage_save_binary_blob_rollover(char *data, uint32_t ptr,uint32_t size, uint32_t rollover);

struct _mode_config_t { char tag[30]; uint32_t *config;};
uint32_t storage_load_config(void);
uint32_t storage_save_config(void);
uint32_t storage_save_mode(const char *filename, struct _mode_config_t *config_t, uint8_t count);
uint32_t storage_load_mode(const char *filename, struct _mode_config_t *config_t, uint8_t count);
bool storage_ls(const char *location, const char *ext);

static const char storage_fat_type_labels[][8]={
    "FAT12",
    "FAT16",
    "FAT32",
    "EXFAT",
    "UNKNOWN"
};
