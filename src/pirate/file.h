bool file_get_args(char *file, size_t file_size);
bool file_close(FIL *file_handle);
bool file_open(FIL *file_handle, const char *file, uint8_t file_status);
bool file_size_check(FIL *file_handle, uint32_t expected_size);
bool file_read(FIL *file_handle, uint8_t *buffer, uint32_t size, uint32_t *bytes_read);
bool file_write(FIL *file_handle, uint8_t *buffer, uint32_t size);
uint32_t file_size(FIL *file_handle);