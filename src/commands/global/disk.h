/**
 * @file disk.h
 * @brief Filesystem operations command interface.
 * @details Provides commands for file/directory operations on TF card or flash.
 */

/**
 * @brief Display file contents (cat command).
 * @param res  Command result structure
 */
void disk_cat_handler(struct command_result* res);

/**
 * @brief Create directory (mkdir command).
 * @param res  Command result structure
 */
void disk_mkdir_handler(struct command_result* res);

/**
 * @brief Change directory (cd command).
 * @param res  Command result structure
 */
void disk_cd_handler(struct command_result* res);

/**
 * @brief Remove file/directory (rm command).
 * @param res  Command result structure
 */
void disk_rm_handler(struct command_result* res);

/**
 * @brief List directory contents (ls command).
 * @param res  Command result structure
 */
void disk_ls_handler(struct command_result* res);

/**
 * @brief Format filesystem.
 * @return 0 on success, error code otherwise
 */
uint8_t disk_format(void);

/**
 * @brief Format filesystem command handler.
 * @param res  Command result structure
 */
void disk_format_handler(struct command_result* res);

/**
 * @brief Set/display volume label.
 * @param res  Command result structure
 */
void disk_label_handler(struct command_result* res);
