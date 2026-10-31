/**
 * @file ui_file_picker.h
 * @brief Reusable fullscreen VT100 file picker for selecting or naming files.
 *
 * Two usage modes:
 *   1. Standalone — opens its own alt-screen overlay, returns selected file.
 *   2. Embedded — runs inside a parent fullscreen app's I/O callbacks.
 *
 * Features:
 *   - Fullscreen scrollable file list with highlighted selection bar
 *   - Up/Down/PageUp/PageDown/Home/End navigation
 *   - Styled centered popup for entering new filenames
 *   - File sizes displayed beside each entry
 *   - Optional extension filtering
 *   - Supports up to UI_FILE_PICKER_MAX files
 *
 * Copyright (c) 2026 Bus Pirate project. MIT License.
 */

#ifndef UI_FILE_PICKER_H
#define UI_FILE_PICKER_H

#include <stdint.h>
#include <stdbool.h>

/** Maximum number of files the picker can display. */
#define UI_FILE_PICKER_MAX 64

/**
 * @brief I/O context for embedded file picker (inside a parent app).
 *
 * Provide the parent app's key-read, write, and repaint callbacks.
 * The file picker will use these instead of managing its own alt-screen.
 */
typedef struct {
    int (*read_key)(void);                          /**< Blocking key read */
    int (*write_out)(int fd, const void* buf, int count); /**< Terminal write */
    void (*repaint)(void);                          /**< Repaint parent screen */
    uint8_t cols;                                   /**< Terminal width */
    uint8_t rows;                                   /**< Terminal height */
} ui_file_picker_io_t;

/**
 * @brief Pick a file from storage using a fullscreen scrollable list.
 *
 * Scans the current directory for files, optionally filters by extension,
 * and presents a fullscreen browsable list with highlighted selection bar.
 * The first entry is "[Enter filename...]" which opens a styled popup
 * for typing a new filename.
 *
 * @param ext       Extension filter (e.g. "bin"), or NULL for all files.
 * @param file_buf  Output buffer for the selected/typed filename.
 * @param buf_size  Size of file_buf (recommend 13 for 8.3 names).
 * @param io        I/O context for embedded use, or NULL for standalone
 *                  (standalone will manage its own alt-screen).
 * @return true if a file was selected/entered, false if cancelled.
 */
bool ui_file_pick(const char* ext,
                  char* file_buf,
                  uint8_t buf_size,
                  const ui_file_picker_io_t* io);

#endif /* UI_FILE_PICKER_H */
