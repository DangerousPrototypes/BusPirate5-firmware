/*
 * hx_build.c — Build wrapper for hx hex editor on Bus Pirate
 *
 * This file defines BUSPIRATE before including all hx source files.
 * The #ifdef BUSPIRATE blocks in the upstream files activate our
 * platform shims (hx_compat.h / hx_compat.c).
 *
 * CMakeLists.txt compiles this file (not the individual .c files).
 * This way the upstream files' only modifications are #ifdef blocks,
 * and updating from upstream is a clean merge.
 *
 * Include order matters:
 *   1. hx_compat.h — must come first to define macros (exit, malloc, etc.)
 *   2. hx_compat.c — shim implementations (arena, I/O, file ops)
 *   3. charbuf.c   — render buffer (uses hx_io_write)
 *   4. util.c      — key reading, window size (uses hx_io_read)
 *   5. undo.c      — undo/redo (pure C, no POSIX)
 *   6. editor.c    — main editor (uses hx_file_*, exit, hx_run/hx_cleanup)
 */
#define BUSPIRATE 1

/* The compat header is included by editor.c under #ifdef BUSPIRATE,
 * but we also need it early for the malloc/exit macros to take effect
 * in all the other files. */
#include "hx_compat.h"

/* Shim implementations */
#include "hx_compat.c"

/* Upstream sources — order does not matter for compilation,
 * but list them in dependency order for clarity. */
#include "charbuf.c"
#include "util.c"
#include "undo.c"
#include "editor.c"
