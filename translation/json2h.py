import json
import sys
import re
import os
from enum import Enum, Flag, auto

# The enumeration values that should not be translated, as they are used
# to select the UI language (and thus should be shown in the language they represent).
#    "T_CONFIG_LANGUAGE_ENGLISH", # "Language - English (US)"
#    "T_CONFIG_LANGUAGE_CHINESE", # "语言 - 中文（简体）"
#    "T_CONFIG_LANGUAGE_POLISH",  # "Język - polski (Polska)"
#    "T_CONFIG_LANGUAGE_BOSNIAN", # "Jezik - bosanski (latinica, Bosna i Hercegovina)"
#    "T_CONFIG_LANGUAGE_ITALIAN", # "Lingua - italiano (Italia)"
#	"T_CONFIG_LANGUAGE",         # "Language / Jezik / Lingua / 语言"
C_NON_TRANSLATED_IDENTIFIERS = r'^T_CONFIG_LANGUAGE(_[a-zA-Z0-9_]+)?$'

# Variable names in C must start with letter or underscore,
# may contain only letters, numbers, and underscores,
# and may not exceed 32 characters.
C_IDENTIFIER_REGEX = r'[a-zA-Z_][a-zA-Z0-9_]{0,31}'

# N.B. - Until Python 3.7, a normal dictionary does not keep insertion order.
#        Require Python 3.7+ explicitly, or enums will randomly shift between builds.
#        or, use `from collections import OrderedDict` to keep insertion order.
#        This would cause random changes in the `enum` order in the generated .h files.
if sys.version_info[0] < 3 or sys.version_info[0] == 3 and sys.version_info[1] < 7:
    print("This script requires Python 3.7 or later.")
    sys.exit(1)


def get_dictionary_of_key_value_pairs_from_header(file_path):
    # Finds strings from the .h file to turn back into JSON:
    # e.g., for lines of the form:
    #      [T_FOO]="Bar",
    #      [T_FOO]    =     "Bar", /* comment */
    #      [T_FOO]  ="Bar", // comment
    # matches[0] = ('T_FOO', 'Bar')


    # BUGBUG - Fails if the string has embedded double quotes:
    #          [T_SOMETHING]="This is a \"test\" string"
    #          Would only match "This is a " instead of the whole string.
    # BUGBUG - Matches even if in commented-out section:
    #          // [T_SOMETHING]="This is a test string"
    #          /*
    #             [T_SOMETHING]="This is a test string"
    #           */
    # BUGBUG - Does not match if the key-value pair is split across multiple lines:
    #          [T_SOMETHING] =
    #              "This is a test string",
    # BUGBUG - old pattern did not restrict identifiers to maximum 32 characters:
    #          [T_CONFIG_LEDS_EFFECT_CLOCKWISEWIPE]="This is a test string"
    #          [T_HELP_UART_BRIDGE_SUPPRESS_LOCAL_ECHO]="Suppress local echo, don't echo back sent data",
    #        // ....-....1....-....2....-....3....-....4
    better_pattern = r'\[([a-zA-Z_][a-zA-Z0-9_]*)\]\s*=\s*\"(.*?)\"'

    key_value_pairs = {}
    with open(file_path, 'r', encoding="utf8") as file:
        lines = file.readlines()
    for line in lines:
        matches = re.findall(better_pattern, line)
        if matches:
            key, value = matches[0]
            if len(key) > 32:
                print(f"WARNING: Key `{key}` is longer than 32 characters, but C requires identifiers to be 32 characters or less.")

            # Still use old key/value pair until test better pattern
            key_value_pairs[key]=value
        else:
            # print("No match found for line:", line.strip())
            pass
    return key_value_pairs

def write_key_value_pairs_to_json_file(file_path, translation_dictionary):
    with open(file_path, 'w', encoding='utf-8') as json_file:
        json.dump(base_translation, json_file, indent=4, ensure_ascii=False)

def write_keys_into_enum_header_template(template_path, header_path, translation_dictionary):

    enum_list_string = ""
    # Iterate over the keys in base_translation
    for index, key in enumerate(base_translation):
        # Check if it's the first iteration -- could remove this if statement,
        # or give all enumerations an explicit value...
        if index == 0:
            enum_list_string += f"    {key}=0,\n"
        else:
            enum_list_string += f"    {key},\n"

    # Read the content of the file
    content = ""
    with open(template_path, 'r', encoding="utf8") as file:
        content = file.read()

    # Replace the tag with the value
    # Yay for memory-inefficient string manipulations!
    content_with_replacement = content.replace("%%%enum_list%%%", enum_list_string)

    # Write base.h
    with open(header_path, 'w', encoding='utf-8') as file:
        # Write the text to the file
        file.write(content_with_replacement)

class DebugJsonConversion(Flag):
    NONE = 0
    UNTRANSLATED = auto()
    IDENTICAL = auto()
    NULL_ENTRY = auto()
    ALL = UNTRANSLATED | IDENTICAL | NULL_ENTRY
    DEFAULT = UNTRANSLATED | IDENTICAL


def convert_remaining_translations_to_h_files(json_directory, header_directory, debug=DebugJsonConversion.NONE):

    # N.B. - Explicitly excludes en-us.json and en-us-POSIX.json
    for filename in os.listdir(json_directory):
        if filename.endswith('.json') and filename != 'en-us.json':
            
            filepath = os.path.join(header_directory, filename)
            with open(filepath, 'r', encoding="utf8") as json_file:

                target_translation = json.load(json_file)
                output_translation={}
                debug_untranslated = []
                debug_identical = []
                debug_null = []

                file_name_without_extension = os.path.splitext(filename)[0]
                variable_name = file_name_without_extension.replace("-", "_")
                # BUGBUG -- verify it's a valid identifier in C
                if not re.findall(C_IDENTIFIER_REGEX, variable_name):
                    print(f"ERROR: `{variable_name}` is not a valid C identifier")
                    exit(1)

                # BUGBUG - check if any keys exist in the translation, but NOT in base ... these are VERY likely errors
                for key in target_translation:
                    if key not in base_translation:
                        print(f"  {file_name_without_extension}: Key `{key}` exists only in the translation.")

                # Loop through the base translations's keys
                for key in base_translation:
                    if not key in target_translation:
                        debug_untranslated.append(key)
                        # BUGBUG -- store NULL to reduce duplicate strings
                        output_translation[key] = base_translation[key]
                    elif target_translation[key] is None:
                        # null in the json means the string was previously reviewed for
                        # translation, but an intentional choice was made to NOT translate it.
                        debug_null.append(key)
                        # BUGBUG -- store NULL to reduce duplicate strings
                        output_translation[key] = base_translation[key]
                    elif base_translation[key] == target_translation[key]:
                        # If the translation includes a key with a translated string that is identical to the base (en-us) string:
                        # Technically this is not an error.  However, if the base string changes (e.g., for clarity or to fix a typo),
                        # the translation .json might be missed in the update.
                        # Therefore, it is likely better to remove the key from the .json file, unless wanting to explicitly "lock in"
                        # a translation, even if the en-us string changes.
                        debug_identical.append(key)
                        # BUGBUG -- store NULL to reduce duplicate strings
                        output_translation[key] = base_translation[key]
                    elif re.match(C_NON_TRANSLATED_IDENTIFIERS, key):
                        # Certain strings, specifically those used to select a language, should never be translated.
                        # These strings in the base language are either multi-lingual, or they are already in the
                        # language they represent (and thus should not be translated).
                        # Enforcing this here prevents accidentally translating these language selection strings.
                        print(f"  {file_name_without_extension}: Key `{key}` should not be translated from `{base_translation[key]}` to `{target_translation[key]}`.")
                        output_translation[key] = base_translation[key]
                    else:
                        output_translation[key] = target_translation[key]

                # Generate the replacement text for the translated text
                translated_h=""
                for key in output_translation:
                    translated_h += f"    [ {key:<32} ] = \"{output_translation[key]}\",\n"

                # Read the content of the template file
                with open('translation.ht', 'r', encoding="utf8") as template_file:
                    content = template_file.read()

                # Replace the tags with the generated values
                content_with_replacement = content.replace("%%%array_data%%%", translated_h)    
                content_with_replacement = content_with_replacement.replace("%%%variable_name%%%", variable_name)

                # Write translation .h file
                with open(file_name_without_extension+'.h', 'w', encoding='utf-8') as output_file:
                    # Write the text to the file
                    output_file.write(content_with_replacement)

                # Print additional debug information if requested
                if debug:
                    if len(debug_null) and DebugJsonConversion.NULL_ENTRY in debug:
                        print("\n=============================================================================")
                        print(f"  {file_name_without_extension}: {len(debug_null)} strings are explicitly not translated:")
                        for k in debug_null:
                            print(f"    [ {k:<32} ] = \"{base_translation[k]}\",")
                        print("")

                    if len(debug_identical) and DebugJsonConversion.IDENTICAL in debug:
                        print("\n=============================================================================")
                        print(f"  {file_name_without_extension}: {len(debug_identical)} strings are identical to the en-us string.")
                        print(f"  Recommend to either: (a) replace with null in .json to indicate choice to not translate it, or")
                        print(f"  (b) remove the entry entirely to indicate the string has not been translated (yet).")
                        for k in debug_identical:
                            print(f"    [ {k:<32} ] = \"{base_translation[k]}\",")
                        print("")

                    if len(debug_untranslated) and DebugJsonConversion.UNTRANSLATED in debug:
                        print("\n=============================================================================")
                        print(f"  {file_name_without_extension}: {len(debug_untranslated)} strings have no entry in the json.")
                        print(f"  These strings likely still require translation.  If intentionally not translating them,")
                        print(f"  add the key with the special json keyword `null`.")
                        for k in debug_untranslated:
                            if re.match(C_NON_TRANSLATED_IDENTIFIERS, k):
                                print(f"    [ {k:<32} ] = null,")
                            else:
                                print(f"    [ {k:<32} ] = \"{base_translation[k]}\",")
                        print("")

# Get the file path from command line arguments
#if len(sys.argv) < 2:
#    print("Usage: python script.py <us-en.h>")
#    sys.exit(1)

# Parse key-value pairs from the first file
print(f"\n======================================================================")
print(f"Obtaining key-value pairs from `en-us.h`")
base_translation = get_dictionary_of_key_value_pairs_from_header('en-us.h')
print(f"\n======================================================================")
print(f"Generating `en-us.json` from `en-us.h`")
write_key_value_pairs_to_json_file('en-us.json', base_translation)
print(f"\n======================================================================")
print(f"Generating `base.h` from `base.ht` with all enumeration keys")
write_keys_into_enum_header_template('base.ht', 'base.h', base_translation)
print(f"\n======================================================================")
print(f"Parsing remaining json translations into corresponding .h files")
convert_remaining_translations_to_h_files('.', '.')
