/**
 * @file file.h
 * @brief File system utility functions.
 * @details Provides wrapper functions for FAT filesystem operations.
 */

/**
 * @brief Get filename argument from user.
 * @param file       Buffer for filename
 * @param file_size  Buffer size
 * @return           true on success
 */
bool file_get_args(char *file, size_t file_size);

/**
 * @brief Close file handle.
 * @param file_handle  File handle to close
 * @return             true on success
 */
bool file_close(FIL *file_handle);

/**
 * @brief Open file.
 * @param file_handle  File handle
 * @param file         Filename
 * @param file_status  File open mode flags
 * @return             true on success
 */
bool file_open(FIL *file_handle, const char *file, uint8_t file_status);

/**
 * @brief Check if file size matches expected size.
 * @param file_handle    File handle
 * @param expected_size  Expected file size in bytes
 * @return               true if size matches
 */
bool file_size_check(FIL *file_handle, uint32_t expected_size);

/**
 * @brief Read data from file.
 * @param file_handle  File handle
 * @param buffer       Data buffer
 * @param size         Bytes to read
 * @param bytes_read   Output bytes actually read
 * @return             true on success
 */
bool file_read(FIL *file_handle, uint8_t *buffer, uint32_t size, uint32_t *bytes_read);

/**
 * @brief Write data to file.
 * @param file_handle  File handle
 * @param buffer       Data buffer
 * @param size         Bytes to write
 * @return             true on success
 */
bool file_write(FIL *file_handle, uint8_t *buffer, uint32_t size);

/**
 * @brief Get file size.
 * @param file_handle  File handle
 * @return             File size in bytes
 */
uint32_t file_size(FIL *file_handle);