# Translation Checker for Bus Pirate 5

This Python script analyzes the `en-us.h` master translation file to help maintain clean and efficient translations.

## Features

1. **Dead Translation Detection**: Identifies translation entries that are defined but never used in the source code
2. **Duplicate Text Detection**: Finds multiple translation keys that have identical text values

## Usage

### Basic Usage

Run the script from the project root directory:

```bash
python3 check_translations.py
```

This will check both unused entries and duplicates, displaying results in the console.

### Command Line Options

- `--help` or `-h`: Show help message
- `-o FILE` or `--output FILE`: Save results to a file instead of console
- `--unused-only`: Only check for unused translation entries
- `--duplicates-only`: Only check for duplicate translation text

### Examples

Check for unused translations only:
```bash
python3 check_translations.py --unused-only
```

Check for duplicates only:
```bash
python3 check_translations.py --duplicates-only
```

Save full report to a file:
```bash
python3 check_translations.py -o report.txt
```

## What It Checks

### Unused Translations

The script scans all `.c`, `.h`, `.cpp`, and `.hpp` files in the `/src` directory (excluding `/src/translation`) to find translation keys that are:
- Defined in `en-us.h`
- Not referenced anywhere in the source code

These may be candidates for removal to reduce translation maintenance overhead.

### Duplicate Translations

The script identifies cases where multiple translation keys have identical text values. This might indicate:
- Opportunities to consolidate keys (if they truly represent the same concept)
- Different contexts requiring the same text (which is fine)
- Copy/paste errors

Each duplicate group shows whether the keys are actually used in the code.

## Configuration

The script has three configurable constants at the top:

```python
TRANSLATION_FILE = "src/translation/en-us.h"  # Master translation file
SOURCE_DIR = "src"                             # Source code directory
EXCLUDE_DIRS = ["src/translation"]             # Directories to exclude
```

## Example Output

```
================================================================================
Bus Pirate 5 Translation Checker
================================================================================

Parsing translation file: src/translation/en-us.h
Found 600 translation entries

Scanning source directory: src
Excluding: src/translation
Found 585 source files to check

--------------------------------------------------------------------------------
CHECKING FOR UNUSED TRANSLATION ENTRIES
--------------------------------------------------------------------------------

Found 88 unused translation entries:

  T_CMDLN_HELP_DISPLAY                     = "hd - show display mode specific help screen."
  T_CMDLN_HELP_MODE                        = "h - show mode specific help screen."
  ...

Summary: 512 used, 88 unused

--------------------------------------------------------------------------------
CHECKING FOR DUPLICATE TRANSLATION TEXT
--------------------------------------------------------------------------------

Found 15 sets of duplicate text:

1. Text: "Data bits"
   Used by 4 keys:
     - T_HWI2C_DATA_BITS_MENU                   [USED]
     - T_HWSPI_BITS_MENU                        [USED]
     - T_I2S_DATA_BITS_MENU                     [USED]
     - T_UART_DATA_BITS_MENU                    [USED]
...

Summary: 15 duplicate text groups

================================================================================
FINAL SUMMARY
================================================================================
Total translations:     600
Used translations:      512
Unused translations:    88
Duplicate text groups:  15
```

## Important Notes

1. **False Positives**: The script may flag some entries as "unused" if they are:
   - Used in preprocessor macros that expand differently
   - Used in generated code
   - Reserved for future use
   
2. **Duplicates**: Having duplicate text is not always a problem. Sometimes the same text is needed in different contexts and should have separate translation keys to allow for context-specific translations in other languages.

3. **Maintenance**: This script should be run periodically during development to keep translations clean and efficient.

## Requirements

- Python 3.6 or higher
- No external dependencies (uses only standard library)
