import json
import sys
import re
import os

# The enumeration values that should not be translated, as they are used
# to select the UI language (and thus should be shown in the language they represent).
DO_NOT_TRANSLATE_THESE_STRINGS = (
    "T_CONFIG_LANGUAGE_ENGLISH", # "Language - English (US)"
    "T_CONFIG_LANGUAGE_CHINESE", # "语言 - 中文（简体）"
    "T_CONFIG_LANGUAGE_POLISH",  # "Język - polski (Polska)"
    "T_CONFIG_LANGUAGE_BOSNIAN", # "Jezik - bosanski (latinica, Bosna i Hercegovina)"
    "T_CONFIG_LANGUAGE_ITALIAN", # "Lingua - italiano (Italia)"
	"T_CONFIG_LANGUAGE",         # "Language / Jezik / Lingua / 语言"
)



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


    # Define the regular expression pattern to find the key-value pairs in the .h file
    pattern = r'\[(.*?)\]="(.*?)"'

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
    better_pattern = r'\[([a-zA-Z_][a-zA-Z0-9_]{0,31})\]\s*=\s*\"(.*?)\"'

    key_value_pairs = {}
    with open(file_path, 'r', encoding="utf8") as file:
        lines = file.readlines()
    for line in lines:
        matches = re.findall(pattern, line)
        if matches:
            key, value = matches[0]

            chk_matches = re.findall(better_pattern, line)
            if not chk_matches:
                print(f"WARNING: Key: `{key}` value `{value}` does not match improved pattern: `{line.strip()}`")
            else:
                chk_key, chk_value = chk_matches[0] 
                mismatch = ""
                if key != chk_key:
                    mismatch += "Key: `{key}` vs. `{chk_key}`,"
                if value != chk_value:
                    mismatch += "Value: `{value}` vs. `{chk_value}`, "
                if len(mismatch) > 0:
                    print(f"WARNING: Mismatch: `{mismatch}` `{line.strip()}`")

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

def convert_remaining_translations_to_h_files(json_directory, header_directory):
    # N.B. - Explicitly excludes en-us.json and en-us-POSIX.json
    for filename in os.listdir(json_directory):
        if filename.endswith('.json') and filename != 'en-us.json':
            
            filepath = os.path.join(header_directory, filename)
            with open(filepath, 'r', encoding="utf8") as json_file:

                target_translation = json.load(json_file)
                output_translation={}

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

                # Generate a new dictionary with same insertion order as base_translation
                for key in base_translation:
                    if not key in target_translation:
                        # print(f"  {file_name_without_extension}: Key `{key}` has no translation.")
                        # BUGBUG -- store NULL to reduce duplicate strings
                        output_translation[key] = base_translation[key]
                    elif base_translation[key] == target_translation[key]:
                        # print(f"  {file_name_without_extension}: Key `{key}` identical to base (this is OK).")
                        # BUGBUG -- store NULL to reduce duplicate strings
                        output_translation[key] = target_translation[key]
                    elif key in DO_NOT_TRANSLATE_THESE_STRINGS:
                        # This simply helps avoid accidentally translating a language selection string
                        # as those should always be shown in the language they represent.
                        print(f"  {file_name_without_extension}: Key `{key}` should not be translated from `{base_translation[key]}` to `{target_translation[key]}`.")
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
