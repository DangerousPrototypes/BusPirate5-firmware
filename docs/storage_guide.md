# Storage & Persistence

> Developer guide for the Bus Pirate storage system — saving and loading mode configuration and system settings via FatFS on NAND flash or TF/microSD card.

---

## Storage Hardware

Bus Pirate supports two storage backends selected at compile time:

- **Rev 9+**: On-board NAND flash (`BP_HW_STORAGE_NAND`)
- **Rev 8**: TF/microSD card slot (`BP_HW_STORAGE_TFCARD`)

Both backends expose a FAT filesystem through the FatFS library. Higher-level APIs in `src/pirate/storage.c` wrap FatFS so that mode and system code never touches raw flash or SD commands directly.

## Mode Config Descriptor

Each protocol mode declares a descriptor table that maps JSON paths to in-memory config variables. The descriptor types are defined in `src/pirate/storage.h`:

```c
typedef enum _mode_config_format {
    MODE_CONFIG_FORMAT_DECIMAL,
    MODE_CONFIG_FORMAT_HEXSTRING,
} mode_config_format_t;

typedef struct _mode_config_t {
    char tag[30];
    uint32_t* config;
    mode_config_format_t formatted_as;
} mode_config_t;
```

| Field | Type | Purpose |
|-------|------|---------|
| `tag` | `char[30]` | JSON path in the config file (e.g. `"$.speed"`) |
| `config` | `uint32_t*` | Pointer to the config variable to save/load |
| `formatted_as` | `mode_config_format_t` | `MODE_CONFIG_FORMAT_DECIMAL` or `MODE_CONFIG_FORMAT_HEXSTRING` |

## Storage Descriptor Pattern

A mode registers its persistent settings by defining a filename and a `mode_config_t` array. From `dummy1.c`:

```c
const char config_file[] = "bpdummy1.bp";
const mode_config_t config_t[] = {
    { "$.speed",  &mode_config.speed,  MODE_CONFIG_FORMAT_DECIMAL },
    { "$.output", &mode_config.output, MODE_CONFIG_FORMAT_DECIMAL },
};
```

The `config_file` string is the filename on the FAT volume. Each entry in `config_t` binds a JSON key to a `uint32_t` field in the mode's static config struct.

## Mode Config API

Two functions handle round-tripping mode settings to disk:

```c
uint32_t storage_save_mode(const char* filename, const mode_config_t* config_t, uint8_t count);
uint32_t storage_load_mode(const char* filename, const mode_config_t* config_t, uint8_t count);
```

| Function | Parameters | Returns |
|----------|-----------|---------|
| `storage_save_mode` | filename, descriptor array, element count | Non-zero on success |
| `storage_load_mode` | filename, descriptor array, element count | Non-zero on success |

## Complete Save/Load Pattern

The standard pattern used inside a mode's `protocol_setup` function (from `dummy1.c`):

```c
// Load saved settings
if (storage_load_mode(config_file, config_t, count_of(config_t))) {
    printf("\r\n\r\n%s%s%s\r\n", ui_term_color_info(),
           GET_T(T_USE_PREVIOUS_SETTINGS), ui_term_color_reset());
    dummy1_settings();
    prompt_result result;
    bool user_value;
    if (!ui_prompt_bool(&result, true, true, true, &user_value)) {
        return 0;
    }
    if (user_value) {
        return 1; // User accepted saved settings
    }
}
// ... wizard prompts ...
storage_save_mode(config_file, config_t, count_of(config_t));
```

The flow is:

1. Attempt to load previously saved settings from the config file.
2. If a file exists, display the loaded values and ask the user whether to reuse them.
3. If the user accepts, return immediately — no wizard needed.
4. Otherwise fall through to the interactive setup wizard.
5. After the wizard completes, save the new settings to disk.

## Global System Config

System-wide settings (terminal color, display brightness, etc.) are persisted with a separate pair of functions:

```c
uint32_t storage_load_config(void);
uint32_t storage_save_config(void);
```

These operate on the global `system_config` struct and use the reserved filename `config.bp`.

## Config File Naming Convention

Each mode stores its settings in a dedicated `.bp` file on the FAT volume:

| Mode | Filename |
|------|----------|
| UART | `bpuart.bp` |
| SPI | `bpspi.bp` |
| I2C | `bpi2c.bp` |
| Dummy1 | `bpdummy1.bp` |
| Global | `config.bp` |

## Other Storage API

Lower-level storage functions for initialization, mounting, and file operations:

```c
void storage_init(void);
bool storage_detect(void);
uint8_t storage_mount(void);
void storage_unmount(void);
uint8_t storage_format(void);
void storage_file_error(uint8_t res);
bool storage_save_binary_blob_rollover(char* data, uint32_t ptr, uint32_t size, uint32_t rollover);
bool storage_file_exists(const char* filepath);
bool storage_ls(const char* location, const char* ext, const uint8_t flags);
```

| Function | Purpose |
|----------|---------|
| `storage_init` | One-time hardware initialization (called at boot) |
| `storage_detect` | Check whether storage media is present |
| `storage_mount` / `storage_unmount` | Mount or unmount the FAT volume |
| `storage_format` | Format the storage media with a fresh FAT filesystem |
| `storage_file_error` | Print a human-readable error for a FatFS `FRESULT` code |
| `storage_save_binary_blob_rollover` | Write a raw binary buffer with ring-buffer rollover support |
| `storage_file_exists` | Return `true` if a file exists at the given path |
| `storage_ls` | List directory contents, optionally filtered by extension and flags |

## FatFS Operations

For anything not covered by the high-level API, the standard FatFS functions are available directly:

- `f_open` / `f_close` — open and close files
- `f_read` / `f_write` — read and write data
- `f_mkdir` — create directories
- `f_unlink` — delete files

Refer to the [FatFS documentation](http://elm-chan.org/fsw/ff/) for full details on return codes and options.

## Important Rules

- **All `mode_config` fields must be `uint32_t`.** The JSON serializer only handles `uint32_t` values. Booleans, enums, and smaller integers must be stored as `uint32_t`.
- **Config struct must be file-static.** Declare it as `static struct { ... } mode_config;` so the pointers in the descriptor table remain valid for the lifetime of the mode.
- **Always save after successful setup.** Call `storage_save_mode` at the end of the wizard so the user's choices persist.
- **Always try to load before the wizard.** Call `storage_load_mode` at the top of `protocol_setup` to offer the user their previous settings.

## Related Documentation

- [new_mode_guide.md](new_mode_guide.md) — Step 4: Storage Descriptor
- [system_config_reference.md](system_config_reference.md) — Global config
- [error_handling_reference.md](error_handling_reference.md) — FRESULT codes
- Source: `src/pirate/storage.h`, `src/pirate/storage.c`
