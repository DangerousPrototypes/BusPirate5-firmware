/**
 * @file file.h
 * @brief File system utility functions.
 * @details Provides wrapper functions for FAT filesystem operations.
 */

struct bp_command_def;

/**
 * @brief Get filename from a flag argument.
 * @param def     Command definition for argument lookup
 * @param flag    Flag character (e.g. 'f')
 * @param buf     Buffer for filename
 * @param maxlen  Buffer size
 * @return        true on success, false on failure (prints error)
 */
bool bp_file_get_name_flag(const struct bp_command_def *def, char flag, char *buf, size_t maxlen);

/**
 * @brief Get filename from a positional argument.
 * @param def      Command definition for argument lookup
 * @param position Positional index (1-based)
 * @param buf      Buffer for filename
 * @param maxlen   Buffer size
 * @return         true on success, false on failure (prints error)
 */
bool bp_file_get_name_positional(const struct bp_command_def *def, uint8_t position, char *buf, size_t maxlen);


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