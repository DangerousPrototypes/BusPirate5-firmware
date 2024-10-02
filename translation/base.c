#include <stdio.h>
#include <string.h> // for strlen()
#include "pico/stdlib.h"
#include "pirate.h"
#include "system_config.h"
#include "translation/en-us.h"
//#include "translation/zh-cn.h"
#include "translation/pl-pl.h"
#include "translation/bs-ba.h"
#include "translation/it-it.h"

char **t; // to be removed when transitioned to accessor function


// TODO: define a type for translation table (just to make it more readable than `const char * const *`)
// TODO: how to typedef a constant array of pointers to constant characters?
//       gcc warns about the assignment of `const char * const (*)[430]` to target type `const char * const *`
typedef const char * const translation_table_t[T_LAST_ITEM_ALWAYS_AT_THE_END];

// TODO: check how json2h.py pulls strings from json files, and if NULL
//       can be used to indicate that a string has no translation.
// ======================================================================
// To add a new translation:
// ======================================================================
// 1. Edit en-us.h to add a new translation string for the new language.
//    (This is necessary due to current config menu structure...)
//    example:
//        [T_CONFIG_LANGUAGE_ESPERANTO] = "Lingvo - esperanto (Mondo)",
//
// 2. Edit json2h.py to:
//    - include the new language enumeration in the tuple of
//      DO_NOT_TRANSLATE_THESE_STRINGS.  This will help ensure
//      the new language selection is not accidentally translated.
//    - if "language" in that language is not similar to an existing
//      string in T_CONFIG_LANGUAGE, add as option.
//    example:
//      Currently, [T_CONFIG_LANGUAGE]="Language / Jezik / Lingua / 语言"
//      Do not add pl-pl "Język" to that string, as it is similar enough
//      to the bs_Latn_BA "Jezik" string.
// 
// 3. Create a new .json file in the translation directory
//    - Recommand starting with en-us.json as a template.
//    - Name the file using the IETF language tag corresponding
//      to the language of the translation.
//    example:
//        cp en-us.json eo-001.json
//
// 4. Populate at least the strings that are translated
//    - There is no requirement to translate all strings.
//    - Translated strings must be in the same order as the en_us file.
//    - If a string is not translated, either exclude from the json file
//      entirely or set its entry to null -- the generated header will
//      then automatically get the most recent version of the en-US string.
//    example:
//        {
//            "T_CONFIG_LANGUAGE_ESPERANTO": null,
//            "T_CONFIG_PIN": "PiN"
//        }
//
// 5. Edit `base.ht` to add the new language to the enum T_language_t.
//    - This will become part of the new generated `base.h` in the next step.
//    example:
//        language_idx_eo_001, // Esperanto
//
// 6. Run `json2h.py` in the translation directory.
//    - Extracts key/value pairs from `en-us.h`
//    - Generates `en-us.json` (for use as a template for new languages)
//    - Generates `base.h` from `base.ht` (template).
//    - Generates `.h` files for each `.json` file in the directory.
//
// 7. Manually update the table below to include the mapping from the
//    new language idx in base.h (base.ht) to the variable in the generated
//    translation table header file.

// BUGBUG: Rename the source .json files to correspond to the proper IETF language tag, and regenerate the .h files.
// BUGBUG: Update json2h.py to prefix the variable name ... e.g., with `t_translation_table_` or similar.
// N.B. - Because this takes address of a variable, this goes into RAM, not ROM ... but only pointers, so only four bytes per language.

static language_idx_t current_language = language_idx_en_us;

static const translation_table_t* translation_tables[COUNT_OF_LANGUAGE_IDX] = {
    [language_idx_en_us]       = &en_us,       // en_US
    [language_idx_pl_pl]       = &pl_pl,       // pl_PL,
    [language_idx_bs_latn_ba]  = &bs_ba,       // bs_Latn_BA,
    [language_idx_it_it]       = &it_it,       // it_IT,
    // [language_idx_zh_cmn_cn]   = &zh_cmn_CN,   // zh_cmn_CN,
};

#define UTF8_POOP_EMOJI_STRING "\xF0\x9F\x92\xA9" // poop emoji (💩) as a UTF-8 encoded, null-terminated string

//TODO: lots of changes to make this more automatic at compile time
void translation_set(language_idx_t language)
{
    if (language >= COUNT_OF_LANGUAGE_IDX) {
        // log an error?  do nothing?
        return;
    }
    current_language = language;
    t = (char **) translation_tables[language]; // TODO: remove this line when transitioned to accessor function
}

//TODO: migrate from direct access to `t[]`, to using this accessor function instead.
//      this will allow for greater safety, troubleshooting, and logging of errors.
const char * GET_T(enum T_translations index) {
    const char * result = NULL;
    if (index < T_LAST_ITEM_ALWAYS_AT_THE_END) {
        translation_table_t * table = translation_tables[current_language];
        result = (*table)[index]; // parenthesis required!
    } else {
        // BUGBUG -- log error if this ever occurs
        result = UTF8_POOP_EMOJI_STRING "OOB_IDX" UTF8_POOP_EMOJI_STRING;
    }

    // if no translated string was provided, but the index was valid,
    // use en-US string instead.  This allows translations to exclude
    // strings that are unchanged from the en-US version, which may reduce
    // the number of duplicate strings in ROM.
    if (result == NULL) {
        translation_table_t * table = translation_tables[language_idx_en_us];
        result = (*table)[index]; // parenthesis required!
    }

    // if the string was null even from the en-US-POSIX table...
    if (result == NULL) {
        // BUGBUG -- need to log an error here
        result = UTF8_POOP_EMOJI_STRING "?" UTF8_POOP_EMOJI_STRING;
    } else if (result != t[index]) {
        // BUGBUG -- print an error here and use poop emojis!
        result = UTF8_POOP_EMOJI_STRING "MISMATCH" UTF8_POOP_EMOJI_STRING;
    }

    return result;
}

void translation_init(void) {
    translation_set(language_idx_en_us);
    t = (char **) &en_us; // TODO: remove this line when transitioned to accessor function
}
