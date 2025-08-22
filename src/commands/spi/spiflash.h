void spiflash_probe(void);
bool spiflash_init(sfud_flash* flash_info);
bool spiflash_erase(sfud_flash* flash_info);
bool spiflash_erase_verify(
    uint32_t start_address, uint32_t end_address, uint32_t buf_size, uint8_t* buf, sfud_flash* flash_info);
bool spiflash_write_test(
    uint32_t start_address, uint32_t end_address, uint32_t buf_size, uint8_t* buf, sfud_flash* flash_info);
bool spiflash_write_verify(
    uint32_t start_address, uint32_t end_address, uint32_t buf_size, uint8_t* buf, sfud_flash* flash_info);
bool spiflash_dump(uint32_t start_address,
                   uint32_t end_address,
                   uint32_t buf_size,
                   uint8_t* buf,
                   sfud_flash* flash_info,
                   const char* file_name);
bool spiflash_load(uint32_t start_address,
                   uint32_t end_address,
                   uint32_t buf_size,
                   uint8_t* buf,
                   sfud_flash* flash_info,
                   const char* file_name);
bool spiflash_verify(uint32_t start_address,
                     uint32_t end_address,
                     uint32_t buf_size,
                     uint8_t* buf,
                     uint8_t* buf2,
                     sfud_flash* flash_info,
                     const char* file_name);
bool spiflash_force_dump(uint32_t start_address,
                         uint32_t end_address,
                         uint32_t buf_size,
                         uint8_t* buf,
                         sfud_flash* flash_info,
                         const char* file_name);
bool spiflash_show_hex(uint32_t buf_size,
                      uint8_t* buf,
                      sfud_flash* flash_info); 