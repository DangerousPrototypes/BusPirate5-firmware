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

typedef const char * const translation_table_t[T_LAST_ITEM_ALWAYS_AT_THE_END];

// To add a new translation / dialect / language:
// ======================================================================
//
// 1. Edit en-us.h to add a string for the new dialect.
//    The enumeration name MUST begin with `T_CONFIG_LANGUAGE_`.
//    For example, to add Esperanto as a new option, it might look like:
//        [T_CONFIG_LANGUAGE_ESPERANTO] = "Lingvo - esperanto (Mondo)",
//
// 2. Review [T_CONFIG_LANGUAGE] in en-us.h.
//    Review the new dialect's word for "language".
//    If significantly different from existing options,
//    modify the string to include the new translation's word for "language".
//    For example, if language in Esperanto was `XYZZY`, it might change:
//        [T_CONFIG_LANGUAGE] = "Language / Jezik / Lingua / 语言"
//    to:
//        [T_CONFIG_LANGUAGE] = "Language / Jezik / Lingua / 语言 / XYZZY"
//
// 3. Create a new .json file in the translation directory, with the base
//    filename corresponding to the IETF language tag for the new dialect.
//    - Recommand starting with en-us.json as a template.
//    - Name the file using the IETF language tag corresponding
//      to the language of the translation.
//    For example, if adding Esperanto:
//        cp en-us.json eo-001.json
//
// 4. Translate or remove the enumerated string from the .json file
//    - There is no requirement to translate all strings.
//    - Translated strings must be in the same order as the en_us file.
//    - If a string is not translated, simply exclude that entry from
//      the json file entirely.
//    - TODO: check if can explicitly set the entry to null
//    - Missing or entries storing null (special json value) will
//      automatically load the most recent version of the en-US string.
//    Made-up words for example Esperanto translation:
//        {
//            "T_CONFIG_LANGUAGE_ESPERANTO": null,
//            "T_CONFIG_PIN": "PyN"
//        }
//
// 5. Edit `base.ht` to add the new language to the enum T_language_t.
//    - This will become part of the new generated `base.h` in the next step.
//    - Name should be `language_idx_` followed by the IETF language tag
//      (replacing dash with underscore)
//    Continuing this example of adding esperanto:
//        language_idx_eo_001, // Esperanto
//
// 6. Run `python ./json2h.py` while in the translation directory.
//    - This parses `en-us.h` to extracts key/value pairs for each string.
//    - Generates `en-us.json` (for use as a template for new languages).
//    - Generates `base.h` from `base.ht` (template) to define enumeration of string IDs.
//    - Generates `.h` files for each `.json` file in the directory
//      (except `en-us.json`, of course, since that was just generated)
//
// 7. Finally, manually update the table below to include the new `T_language_t`
//    that was added to `base.ht` above, and the name of the table in the new
//    dialect's generated header.
//    Continuing the example of adding Esperanto, the added line might be:
//     [language_idx_eo_001] = &eo_001, // eo-001 aka Esperanto
//
// TODO: lots of changes to make this more automatic at compile time


// BUGBUG: Rename the source .json files to correspond to the proper IETF language tag, and regenerate the .h files.
// BUGBUG: Update json2h.py to prefix the resulting variable name ... e.g., with `t_translation_table_` or similar.



static language_idx_t current_language = language_idx_en_us;

// N.B. - The translation table itself takes up RAM because it stores address of other variables.
//        But this is only four bytes per language.  The actual string array should be in ROM.
static const translation_table_t* translation_tables[COUNT_OF_LANGUAGE_IDX] = {
    [language_idx_en_us]       = &en_us,       // en_US
    [language_idx_pl_pl]       = &pl_pl,       // pl_PL,
    [language_idx_bs_latn_ba]  = &bs_ba,       // bs_Latn_BA,
    [language_idx_it_it]       = &it_it,       // it_IT,
    // [language_idx_zh_cmn_cn]   = &zh_cmn_CN,   // zh_cmn_CN,
};

#define UTF8_POOP_EMOJI_STRING "\xF0\x9F\x92\xA9" // poop emoji (💩) as a UTF-8 encoded, null-terminated string

void translation_set(language_idx_t language)
{
    if (language >= COUNT_OF_LANGUAGE_IDX) {
        // log an error?  do nothing?
        return;
    }
    current_language = language;
}

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
    }

    return result;
}

void translation_init(void) {
    translation_set(language_idx_en_us);
}
