/**
 * @file eeprom_i2c_gui.c
 * @brief Fullscreen interactive GUI for I2C EEPROM operations.
 *
 * Thin application layer on top of ui_mem_gui — defines fields, menus,
 * and an execute callback.  The reusable framework handles the rest:
 * alt-screen lifecycle, config bar, menu bar, hex editor, progress/messages.
 *
 * Copyright (c) 2026 Bus Pirate project. MIT License.
 * Modified by: rewrite using ui_mem_gui framework.
 */

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "pirate/mem.h"
#include "command_struct.h"
#include "fatfs/ff.h"
#include "ui/ui_mem_gui.h"
#include "ui/ui_app.h"
#include "lib/hx/hx_compat.h"
#include "lib/hx/editor.h"
#include "binmode/fala.h"
#include "mode/hwi2c.h"
#include "eeprom_base.h"
#include "eeprom_i2c_gui.h"

/* ── Action IDs for extra menus ─────────────────────────────────────── */

enum {
    /* Action menu (1-6) */
    ACT_DUMP  = 1, ACT_ERASE, ACT_WRITE, ACT_READ, ACT_VERIFY, ACT_TEST,

    /* Device menu (10 + device index) */
    ACT_DEV_BASE = 10,

    /* Auto-detect size */
    ACT_DETECT = 50,

    /* File menu */
    ACT_FILE_BROWSE = 100,

    /* Options menu (app-injected) */
    ACT_I2C_ADDR = 200,
};

/* ── App context ────────────────────────────────────────────────────── */

static const char *const action_names[] = {
    "dump", "erase", "write", "read", "verify", "test",
};
#define ACTION_COUNT 6

typedef struct {
    int      action;         /* -1 = not set, else 0..5 */
    int      device_idx;     /* -1 = not set */
    char     file_name[13];
    uint8_t  i2c_addr;       /* 7-bit, default 0x50 */
    bool     verify_flag;

    const struct eeprom_device_t *devices;
    uint8_t device_count;

    /* Device name pointers for the spinner (built at init).
     * +1 for the "Auto-Detect" sentinel appended at the end. */
    const char *dev_names[17];

    bool auto_detected;   /* true when execute resolved device via auto-detect */
    char result_buf[64];  /* formatted result string for auto-detect success */
} i2c_ctx_t;

static i2c_ctx_t ctx;

/* ── Field indices ──────────────────────────────────────────────────── */

enum {
    FLD_ACTION = 0,
    FLD_DEVICE,
    FLD_FILE,
    FLD_ADDR,
    FLD_VERIFY,
    FLD_EXECUTE,
    FLD_COUNT,
};

/* ── Getters / setters ──────────────────────────────────────────────── */

static int  get_action(void *c)          { return ((i2c_ctx_t *)c)->action; }
static void set_action(void *c, int v)   { ((i2c_ctx_t *)c)->action = v; }
static int  get_device(void *c)          { return ((i2c_ctx_t *)c)->device_idx; }
static void set_device(void *c, int v)   { ((i2c_ctx_t *)c)->device_idx = v; }
static int  get_addr(void *c)            { return ((i2c_ctx_t *)c)->i2c_addr; }
static void set_addr(void *c, int v)     { ((i2c_ctx_t *)c)->i2c_addr = (uint8_t)v; }
static int  get_verify(void *c)          { return ((i2c_ctx_t *)c)->verify_flag ? 1 : 0; }
static void set_verify(void *c, int v)   { ((i2c_ctx_t *)c)->verify_flag = !!v; }
static const char *get_file(void *c)     { return ((i2c_ctx_t *)c)->file_name; }
static void set_file(void *c, const char *s) {
    strncpy(((i2c_ctx_t *)c)->file_name, s, 12);
    ((i2c_ctx_t *)c)->file_name[12] = '\0';
}

static void on_file_change(void *c) {
    i2c_ctx_t *x = (i2c_ctx_t *)c;
    if (x->file_name[0]) {
        hx_embed_load(x->file_name);
    }
}

/* File field is only visible when action needs a file (write/read/verify). */
static bool file_visible(void *c) {
    int a = ((i2c_ctx_t *)c)->action;
    return (a == 2 || a == 3 || a == 4);
}

/* Execute button ready when key fields are set. */
static bool exec_ready(void *c) {
    i2c_ctx_t *x = (i2c_ctx_t *)c;
    if (x->action < 0 || x->device_idx < 0) return false;
    /* auto-detect sentinel is valid only for dump (action 0) */
    if (x->device_idx == x->device_count) return (x->action == 0);
    if (file_visible(c) && x->file_name[0] == '\0') return false;
    return true;
}

/* ── Execute button activation → trigger framework execute ──────────── */

static void on_execute(void *c) {
    (void)c;
    ui_mem_gui_request_execute();
}

/* ── Field definitions ──────────────────────────────────────────────── */

static const ui_field_def_t i2c_fields[FLD_COUNT] = {
    [FLD_ACTION] = {
        .type = UI_FIELD_SPINNER,
        .width = 9,
        .spinner = { .options = action_names, .count = ACTION_COUNT, .wrap = true },
        .get_int = get_action,
        .set_int = set_action,
    },
    [FLD_DEVICE] = {
        .type = UI_FIELD_SPINNER,
        .width = 11,
        .spinner = { .options = NULL, .count = 0, .wrap = true }, /* patched at init */
        .get_int = get_device,
        .set_int = set_device,
    },
    [FLD_FILE] = {
        .type = UI_FIELD_FILE,
        .width = 12,
        .file = { .ext_filter = NULL },
        .get_str = get_file,
        .set_str = set_file,
        .visible = file_visible,
        .on_change = on_file_change,
    },
    [FLD_ADDR] = {
        .type = UI_FIELD_NUMBER,
        .width = 5,
        .number = { .min = 0, .max = 0x7F, .fmt = "0x%02X",
                     .popup_title = "I2C Address (7-bit)" },
        .get_int = get_addr,
        .set_int = set_addr,
    },
    [FLD_VERIFY] = {
        .type = UI_FIELD_CHECKBOX,
        .width = 4,
        .checkbox = { .text = "Vfy" },
        .get_int = get_verify,
        .set_int = set_verify,
    },
    [FLD_EXECUTE] = {
        .type = UI_FIELD_BUTTON,
        .width = 8,
        .button = { .text = "Execute", .ready = exec_ready },
        .on_activate = on_execute,
    },
};

/* ── Extra menus: Action, Device, File ──────────────────────────────── */

static const vt100_menu_item_t action_menu_items[] = {
    { "Dump",   NULL, ACT_DUMP,   0 },
    { "Erase",  NULL, ACT_ERASE,  0 },
    { "Write",  NULL, ACT_WRITE,  0 },
    { "Read",   NULL, ACT_READ,   0 },
    { "Verify", NULL, ACT_VERIFY, 0 },
    { "Test",   NULL, ACT_TEST,   0 },
};

/* Device menu — built dynamically for category separators + size hints.
 * +2 for the auto-detect entry and its leading separator. */
static vt100_menu_item_t dev_menu_items[20];
static char dev_size_hints[16][8];

static void format_size(uint32_t bytes, char *buf, uint8_t buf_size) {
    if (bytes < 1024)
        snprintf(buf, buf_size, "%luB", (unsigned long)bytes);
    else if (bytes < 1024 * 1024)
        snprintf(buf, buf_size, "%luK", (unsigned long)(bytes / 1024));
    else
        snprintf(buf, buf_size, "%luM", (unsigned long)(bytes / (1024 * 1024)));
}

static uint8_t build_device_menu_items(void) {
    uint8_t n = ctx.device_count;
    if (n > 16) n = 16;
    uint8_t idx = 0;
    uint8_t prev_cat = 0;

    for (uint8_t i = 0; i < n; i++) {
        uint8_t cat = (ctx.devices[i].size_bytes <= 2048) ? 1
                    : (ctx.devices[i].size_bytes <= 65536) ? 2 : 3;
        if (prev_cat && cat != prev_cat) {
            dev_menu_items[idx] = (vt100_menu_item_t){
                NULL, NULL, 0, MENU_ITEM_SEPARATOR };
            idx++;
        }
        prev_cat = cat;

        format_size(ctx.devices[i].size_bytes, dev_size_hints[i],
                     sizeof(dev_size_hints[i]));
        dev_menu_items[idx] = (vt100_menu_item_t){
            ctx.devices[i].name, dev_size_hints[i],
            ACT_DEV_BASE + i, 0 };
        idx++;
    }

    /* auto-detect entry at the bottom, separated from the device list */
    dev_menu_items[idx++] = (vt100_menu_item_t){ NULL, NULL, 0, MENU_ITEM_SEPARATOR };
    dev_menu_items[idx++] = (vt100_menu_item_t){ "Auto-Detect", NULL, ACT_DETECT, 0 };
    return idx;
}

static vt100_menu_item_t file_menu_items[] = {
    { "Browse storage...", NULL, ACT_FILE_BROWSE, 0 },
};

static const vt100_menu_item_t i2c_option_items[] = {
    { "I2C Address", NULL, ACT_I2C_ADDR, 0 },
};

/* ── Menu dispatch ──────────────────────────────────────────────────── */

static void menu_dispatch(void *c, int action_id) {
    i2c_ctx_t *x = (i2c_ctx_t *)c;

    /* Action menu */
    if (action_id >= ACT_DUMP && action_id <= ACT_TEST) {
        x->action = action_id - ACT_DUMP;
        return;
    }

    /* Device menu */
    if (action_id >= ACT_DEV_BASE &&
        action_id < ACT_DEV_BASE + x->device_count) {
        x->device_idx = action_id - ACT_DEV_BASE;
        return;
    }

    /* Auto-detect — set spinner to sentinel, detection runs in execute_cb */
    if (action_id == ACT_DETECT) {
        x->device_idx = x->device_count;
        return;
    }

    /* File browse — open the file picker and load result into hex editor */
    if (action_id == ACT_FILE_BROWSE) {
        char file_buf[13] = {0};
        if (ui_mem_gui_browse_file(file_buf, sizeof(file_buf))) {
            strncpy(x->file_name, file_buf, 12);
            x->file_name[12] = '\0';
            hx_embed_load(x->file_name);
        }
        return;
    }

    /* I2C Address — open number popup */
    if (action_id == ACT_I2C_ADDR) {
        uint32_t result;
        if (ui_mem_gui_popup_number("I2C Address (7-bit)",
                                    x->i2c_addr, 0, 0x7F, &result)) {
            x->i2c_addr = (uint8_t)result;
        }
        return;
    }
}

/* ── Callbacks for ui_mem_gui_config_t ──────────────────────────────── */

static bool cfg_ready(void *c) {
    return exec_ready(c);
}

static bool pre_check(void *c, const char **err) {
    (void)c;
    if (i2c_mode_config.clock_stretch) {
        *err = "I2C clock stretching enabled — re-enter mode with it DISABLED";
        return false;
    }
    return true;
}

static bool needs_confirm(void *c) {
    int a = ((i2c_ctx_t *)c)->action;
    /* erase=1, write=2, test=5 are destructive */
    return (a == 1 || a == 2 || a == 5);
}

/* Bridge: translate ui_mem_gui_ops_t → eeprom_ui_ops_t for the action fns. */
static bool execute_cb(void *c, const ui_mem_gui_ops_t *ops, const char **result) {
    i2c_ctx_t *x = (i2c_ctx_t *)c;

    /* auto-detect: probe chip size read-only via address mirroring.
     * on success, device_idx is resolved to a real entry and we fall
     * through to the normal execute path. */
    if (x->device_idx == x->device_count) {
        ops->message("Probing chip size (read-only)...", ops->ctx);
        int detected = eeprom_i2c_detect_size(x->i2c_addr,
                                              x->devices,
                                              x->device_count,
                                              ops);
        if (detected == -3) {
            ops->error("Auto-detect failed: I2C error - check wiring and mode", ops->ctx);
            *result = "Auto-detect: I2C error";
            return false;
        }
        if (detected == -1) {
            ops->error("Auto-detect failed: uniform data at address 0 - "
                       "chip may be blank, select device manually", ops->ctx);
            *result = "Auto-detect: uniform data";
            return false;
        }
        if (detected == -2) {
            ops->error("Auto-detect failed: ambiguous result - select device manually", ops->ctx);
            *result = "Auto-detect: ambiguous";
            return false;
        }
        /* resolved - update spinner and fall through */
        x->device_idx = detected;
        x->auto_detected = true;
        char msg[64];
        uint32_t sz = x->devices[detected].size_bytes;
        if (sz >= 1024)
            snprintf(msg, sizeof(msg), "Detected: %s (%luKB)",
                     x->devices[detected].name, (unsigned long)(sz / 1024));
        else
            snprintf(msg, sizeof(msg), "Detected: %s (%luB)",
                     x->devices[detected].name, (unsigned long)sz);
        ops->message(msg, ops->ctx);
    }

    /* Build eeprom_info with current configuration */
    struct eeprom_info eeprom;
    memset(&eeprom, 0, sizeof(eeprom));
    eeprom.device         = &x->devices[x->device_idx];
    eeprom.device_address = x->i2c_addr;
    eeprom.action         = (uint32_t)x->action;
    eeprom.verify_flag    = x->verify_flag;
    strncpy(eeprom.file_name, x->file_name, sizeof(eeprom.file_name) - 1);

    /* Bridge the ops — the function signatures are compatible. */
    eeprom_ui_ops_t ui_ops = {
        .progress = ops->progress,
        .message  = ops->message,
        .error    = ops->error,
        .warning  = ops->warning,
        .ctx      = ops->ctx,
    };
    eeprom.ui = &ui_ops;

    /* Show chip info */
    {
        char info[80];
        snprintf(info, sizeof(info), "%s: %d bytes, %d byte pages, addr %d bytes",
                 eeprom.device->name, eeprom.device->size_bytes,
                 eeprom.device->page_bytes, eeprom.device->address_bytes);
        ops->message(info, ops->ctx);
    }

    bool success = false;
    char buf[EEPROM_ADDRESS_PAGE_SIZE];
    uint8_t verify_buf[EEPROM_ADDRESS_PAGE_SIZE];

    switch (x->action) {
    case 0: { /* dump → hex editor */
        uint32_t eep_size = eeprom.device->size_bytes;
        uint32_t blocks   = eeprom_get_address_blocks_total(&eeprom);
        uint32_t bsz      = eeprom_get_address_block_size(&eeprom);

        if (eep_size <= HX_PAGED_THRESHOLD) {
            char *dump_buf = hx_arena_malloc(eep_size);
            if (!dump_buf) {
                ops->error("Not enough memory for dump", ops->ctx);
            } else {
                bool err = false;
                for (uint32_t i = 0; i < blocks; i++) {
                    ops->progress(i, blocks, ops->ctx);
                    if (eeprom.device->hal->read(&eeprom, i * 256,
                            bsz, (uint8_t *)&dump_buf[i * 256])) {
                        char m[60];
                        snprintf(m, sizeof(m), "Read error at %lu",
                                 (unsigned long)(i * 256));
                        ops->error(m, ops->ctx);
                        hx_arena_free(dump_buf);
                        err = true;
                        break;
                    }
                }
                if (!err) {
                    ops->progress(blocks, blocks, ops->ctx);
                    hx_embed_load_buffer(dump_buf, eep_size,
                                         eeprom.device->name);
                    success = true;
                }
            }
        } else {
            strncpy(eeprom.file_name, "~dump.bin",
                    sizeof(eeprom.file_name) - 1);
            ops->message("Reading EEPROM to temp file...", ops->ctx);
            if (!eeprom_read(&eeprom, buf, sizeof(buf),
                    (char *)verify_buf, sizeof(verify_buf),
                    EEPROM_READ_TO_FILE)) {
                hx_embed_load("~dump.bin");
                success = true;
            }
        }
        break;
    }
    case 1: /* erase */
        success = !eeprom_action_erase(&eeprom, (uint8_t *)buf, sizeof(buf),
                      verify_buf, sizeof(verify_buf), x->verify_flag);
        break;
    case 2: /* write */
        success = !eeprom_action_write(&eeprom, (uint8_t *)buf, sizeof(buf),
                      verify_buf, sizeof(verify_buf), x->verify_flag);
        break;
    case 3: /* read */
        success = !eeprom_action_read(&eeprom, (uint8_t *)buf, sizeof(buf),
                      verify_buf, sizeof(verify_buf), x->verify_flag);
        break;
    case 4: /* verify */
        success = !eeprom_action_verify(&eeprom, (uint8_t *)buf, sizeof(buf),
                      verify_buf, sizeof(verify_buf));
        break;
    case 5: /* test */
        success = !eeprom_action_test(&eeprom, (uint8_t *)buf, sizeof(buf),
                      verify_buf, sizeof(verify_buf));
        break;
    }

    if (success && x->auto_detected) {
        uint32_t sz = x->devices[x->device_idx].size_bytes;
        if (sz >= 1024)
            snprintf(x->result_buf, sizeof(x->result_buf),
                     "Success! - Detected %s (%luKB)",
                     x->devices[x->device_idx].name,
                     (unsigned long)(sz / 1024));
        else
            snprintf(x->result_buf, sizeof(x->result_buf),
                     "Success! - Detected %s (%luB)",
                     x->devices[x->device_idx].name,
                     (unsigned long)sz);
        *result = x->result_buf;
    } else {
        *result = success ? "Last operation: Success :)"
                          : "Last operation: FAILED";
    }
    return success;
}

static void post_exec_load(void *c) {
    i2c_ctx_t *x = (i2c_ctx_t *)c;
    /* After read action, load the output file into hex viewer */
    if (x->action == 3 && x->file_name[0]) {
        hx_embed_load(x->file_name);
    }
    /* Dump loads are handled inline in execute_cb */
}

/* ── Public entry point ─────────────────────────────────────────────── */

bool eeprom_i2c_gui(const struct eeprom_device_t *devices,
                     uint8_t device_count,
                     struct eeprom_info *args) {
    (void)args;

    /* Init context */
    memset(&ctx, 0, sizeof(ctx));
    ctx.action        = -1;
    ctx.i2c_addr      = 0x50;
    ctx.devices       = devices;
    ctx.device_count  = device_count;
    ctx.device_idx    = device_count; /* default to auto-detect sentinel */
    ctx.auto_detected = false;

    /* Build device name pointer table for the spinner.
     * Last slot is the "Auto-detect" sentinel entry. */
    uint8_t n = device_count;
    if (n > 16) n = 16;
    for (uint8_t i = 0; i < n; i++) {
        ctx.dev_names[i] = devices[i].name;
    }
    ctx.dev_names[n] = "Auto";

    /* Patch the device spinner with the actual device list.
     * Field defs are const, so we need a mutable copy. */
    ui_field_def_t fields[FLD_COUNT];
    memcpy(fields, i2c_fields, sizeof(fields));
    fields[FLD_DEVICE].spinner.options = ctx.dev_names;
    fields[FLD_DEVICE].spinner.count   = n + 1; /* +1 for Auto-detect */

    /* Build device menu items */
    uint8_t dev_item_count = build_device_menu_items();

    /* Extra menus: Action, Device, File */
    vt100_menu_def_t extra_menus[3] = {
        { "Action", (vt100_menu_item_t *)action_menu_items, ACTION_COUNT },
        { "Device", dev_menu_items, dev_item_count },
        { "File",   file_menu_items, 1 },
    };

    ui_mem_gui_config_t config = {
        .title             = "I2C EEPROM",
        .fields            = fields,
        .field_count       = FLD_COUNT,
        .extra_menus       = extra_menus,
        .extra_menu_count  = 3,
        .execute           = execute_cb,
        .needs_confirm     = needs_confirm,
        .pre_check         = pre_check,
        .config_ready      = cfg_ready,
        .post_execute_load = post_exec_load,
        .menu_dispatch     = menu_dispatch,
        .option_items      = i2c_option_items,
        .option_item_count = 1,
        .enable_hex_editor = true,
        .ctx               = &ctx,
    };

    return ui_mem_gui_run(&config);
}

