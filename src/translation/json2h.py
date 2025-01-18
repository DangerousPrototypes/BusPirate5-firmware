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

C_FORMAT_SPECIFIERS_TO_DATA_TYPE = {
    # TODO: entry should be a tuple, with second and later elements being "acceptable" equivalents (e.g., larger sized, pvoid, etc.
    # See https://en.cppreference.com/w/c/io/fprintf
    '*'   : 'as_width_or_precision',
    's'   : 'as_char_ptr',
    'ls'  : 'as_wchar_ptr',
    'td'  : 'as_ptrdiff_t',
    'ti'  : 'as_ptrdiff_t',
    'zd'  : 'as_ssizet',
    'zi'  : 'as_ssizet',
    'jd'  : 'as_intmax_t',
    'ji'  : 'as_intmax_t',
    'lld' : 'as_longlong',
    'lli' : 'as_longlong',
    'ld'  : 'as_long',
    'li'  : 'as_long',
    'c'   : 'as_int',
    'd'   : 'as_int',
    'i'   : 'as_int',
    'hd'  : 'as_short',
    'hi'  : 'as_short',
    'hhd' : 'as_schar',
    'hhi' : 'as_schar',
    'to'  : 'as_uptrdiff_t',
    'tx'  : 'as_uptrdiff_t',
    'tX'  : 'as_uptrdiff_t',
    'tu'  : 'as_uptrdiff_t',
    'zo'  : 'as_size_t',
    'zx'  : 'as_size_t',
    'zX'  : 'as_size_t',
    'zu'  : 'as_size_t',
    'jo'  : 'as_uintmax_t',
    'jx'  : 'as_uintmax_t',
    'jX'  : 'as_uintmax_t',
    'ju'  : 'as_uintmax_t',
    'llo' : 'as_ulonglong',
    'llx' : 'as_ulonglong',
    'llX' : 'as_ulonglong',
    'llu' : 'as_ulonglong',
    'lo'  : 'as_ulong',
    'lx'  : 'as_ulong',
    'lX'  : 'as_ulong',
    'lu'  : 'as_ulong',
    'o'   : 'as_uint',
    'x'   : 'as_uint',
    'X'   : 'as_uint',
    'u'   : 'as_uint',
    'ho'  : 'as_ushort',
    'hx'  : 'as_ushort',
    'hX'  : 'as_ushort',
    'hu'  : 'as_ushort',
    'hho' : 'as_uchar',
    'hhx' : 'as_uchar',
    'hhX' : 'as_uchar',
    'hhu' : 'as_uchar',
    'f'   : 'as_double',
    'F'   : 'as_double',
    'e'   : 'as_double',
    'E'   : 'as_double',
    'g'   : 'as_double',
    'G'   : 'as_double',
    'a'   : 'as_double',
    'A'   : 'as_double',
    'lf'  : 'as_double',
    'lF'  : 'as_double',
    'le'  : 'as_double',
    'lE'  : 'as_double',
    'lg'  : 'as_double',
    'lG'  : 'as_double',
    'la'  : 'as_double',
    'lA'  : 'as_double',
    'Lf'  : 'as_ldouble',
    'LF'  : 'as_ldouble',
    'Le'  : 'as_ldouble',
    'LE'  : 'as_ldouble',
    'Lg'  : 'as_ldouble',
    'LG'  : 'as_ldouble',
    'La'  : 'as_ldouble',
    'LA'  : 'as_ldouble',
    'p'   : 'as_void_ptr',
    # 'UNSAFE'        : ('hhn', 'hn', 'n', 'ln', 'lln', 'jn', 'zn', 'tn'),  # writes TO a pointer, count of characters written thus far....  DO NOT ALLOW IN THIS CODEBASE!
}

def CanonicalizeTranslationRecord(object):
    if isinstance(object, str):
        # DataTypes in the JSON makes easier for webpage to show helpful info
        datatypes = list(format_specifiers_iterable(object))
        return {'Localized': object, 'EN_US': None, 'Comments': None, 'DataTypes': datatypes }
    elif isinstance(object, dict):
        if not 'Localized' in object:
            raise ValueError("'Localized' is a mandatory key")
        for k, v in object.items():
            match k:
                case 'Localized':
                    if v is None:
                        raise ValueError("'Localized' value cannot be None")
                    if not isinstance(v, str):
                        raise TypeError("'Localized' value must be a string")
                case 'EN_US':
                    if v is not None and not isinstance(v, str):
                        raise TypeError("'EN_US' value, if exists, must be a string")
                case 'Comments':
                    if v is not None and not isinstance(v, str):
                        raise TypeError("'Comments' value, if exists, must be a string")
                case 'DataTypes':
                    if v is not None:
                        if not isinstance(v, list):
                            raise TypeError("'DataTypes' value, if exists, must be a list of format specifier strings (not a string itself)")
                        for item in v:
                            if not isinstance(item, str):
                                raise TypeError("'DataTypes' list must contain only strings")
                            if not item in C_FORMAT_SPECIFIERS_TO_DATA_TYPE:
                                raise ValueError(f"DataType '{item}' is not a valid C format specifier")
                case _:
                    print(f"WARNING: Unknown key '{k}' with value '{v}'")
                    # raise ValueError(f"Unknown key '{k}' with value '{v}'")
        datatypes = list(format_specifiers_iterable(object['Localized']))
        if object['DataTypes'] != datatypes:
            raise ValueError(f"DataTypes in object does not match actual format specifiers in the Localized string")
        return object
    else:
        raise TypeError("object must be either a string or a dictionary")
def CanonicalizeEntireTranslation(object):
    if not isinstance(object, dict):
        raise TypeError("object must be a dictionary")
    for k, v in object.items():
        if not isinstance(k, str):
            raise TypeError(f"each key in dictionary must be a string")
        if not isinstance(v, dict):
            raise TypeError(f"each value in dictionary must be a dictionary")
        object[k] = CanonicalizeTranslationRecord(v)
    return object
def describe_differences_between_translation_records(old_record, new_record):
    if not isinstance(old_record, dict):
        raise TypeError("old_record must be a dictionary")
    if not isinstance(new_record, dict):
        raise TypeError("new_record must be a dictionary")
    if not 'Localized' in old_record:
        raise ValueError("'Localized' is a mandatory key")
    diffs = ""
    for k, v in old_record.items():
        if not k in new_record:
            diffs += f"Key '{k}' is missing from the new record\n"
        elif v is None and new_record[k] is not None:
            diffs += f"Key '{k}' is None in the old record, but not None in the new record\n"
        if v != new_record[k]:
            diffs += f"Key '{k}' has different values; old: `{old_record[k]}`  vs. new `{new_record[k]}`\n"
    if len(diffs) == 0:
        return None
    return diffs
def describe_format_specifier_differences_between_translation_records(old_record, new_record):
    err_msg = ""
    if old_record is None:
        err_msg += f"  old record is None\n"
    if new_record is None:
        err_msg += f"  new record is None\n"
    if len(err_msg) > 0:
        return err_msg
    if not isinstance(old_record, dict):
        err_msg += f"  old record is not a dict\n"
    if not isinstance(new_record, dict):
        err_msg += f"  new record is not a dict\n"
    if len(err_msg) > 0:
        return err_msg
    if not 'DataTypes' in old_record:
        err_msg += f"  old record is missing 'DataTypes'\n"
    if not 'DataTypes' in new_record:
        err_msg += f"  new record is missing 'DataTypes'\n"
    if len(err_msg) > 0:
        return err_msg
    old_list = old_record['DataTypes']
    new_list = new_record['DataTypes']
    if len(old_list) != len(new_list):
        return f"  format specifier count mismatch, old ({len(old_list)}) != new ({len(new_list)})\n"
    for i in range(len(old_list)):
        if old_list[i] is None:
            err_msg += f"  idx {i}: old is None\n"
        if new_list[i] is None:
            err_msg += f"  idx {i}: new is None\n"
        if old_list[i] != new_list[i]:
            err_msg += f"  idx {i}: mismatched type, old: `{old_list[i]}` vs. new: `{new_list[i]}`\n"
    if len(err_msg) > 0:
        return err_msg
    return None


def differences_between_translations(old_translation, new_translation):
    if not isinstance(old_translation, dict):
        raise TypeError("old_base_translation must be a dictionary")
    if not isinstance(new_translation, dict):
        raise TypeError("new_translation must be a dictionary")
    # NOTE: It's fine to have extra keys in the new translation
    #       Just want to catch existing strings that are being modified
    err_msg = ""
    for key in old_translation:
        if not isinstance(key, str):
            raise TypeError(f"each key in dictionary must be a string")
        if key not in new_translation:
            err_msg += f"  Key `{key}` is missing from the new translation.\n"
        else:
            diffs = describe_differences_between_translation_records(old_translation[key], new_translation[key])
            if diffs is not None:
                err_msg += f"  Key `{key}` has differences:"
                err_msg += diffs
    if len(err_msg) > 0:
        return err_msg
    return None
def differences_between_translation_format_specifiers(old_translation, new_translation):
    if not isinstance(old_translation, dict):
        raise TypeError("old_base_translation must be a dictionary")
    if not isinstance(new_translation, dict):
        raise TypeError("new_translation must be a dictionary")
    # Note: It's fine to have extra keys in the new translation
    #       Just want to catch existing strings that are being modified
    err_msg = ""
    for key in old_translation:
        if not isinstance(key, str):
            raise TypeError(f"each key in dictionary must be a string")
        if key not in new_translation:
            err_msg += f"  Key `{key}` is missing from the new translation.\n"
            continue
        old_record = old_translation[key]
        new_record = new_translation[key]
        diffs = describe_format_specifier_differences_between_translation_records(old_record, new_record)
        if diffs is not None:
            err_msg += f"  Key `{key}` has format specifier differences:"
            err_msg += diffs
    if len(err_msg) > 0:
        return err_msg
    return None

class MyRegex:
    # define identifiers that should not have translations
    # TODO: hard-code these strings directly in translations/base.c?
    _C_NON_TRANSLATED_IDENTIFIERS = r'^T_CONFIG_LANGUAGE(_[a-zA-Z0-9_]+)?$'

    # this matches C-style variable names
        # Variable names in C must start with letter or underscore,
        # may contain only letters, numbers, and underscores,
        # and may not exceed 32 characters.
    _C_IDENTIFIER_REGEX_STR = r'[a-zA-Z_][a-zA-Z0-9_]{0,31}'

    # The strings in en-us.h may have format specifiers.
        # In order to check that other translations use the same types (and counts)
        # it is necessary to find them all.
        # Here's a breakdown of the regex:
        #
        # %
        #     The start of the format specifier
        # (?P<flags>[0 #+-]?)
        #     Optional flags for the format specifier; can be empty
        # (?P<width>(\*|[0-9]*))
        #     Width is either a single '*' (requiring another int parameter), or sequence of digits, or empty
        # ((\.)(?P<precision>(\*|[0-9]*)))
        #     Precision is after the '.' character; can be empty
        # (?P<data_type>([hl]{0,2}|[jztL])?[diuoxXeEfFgGaAcpsSn])
        #     Technically, only the last character is the data type
        #     Captures the length modifier and data type
        # 
        # NOTE -- after finding a match, caller must backtrack
        #         counting consecutive prior blackslash chars and
        #         counting consecutive prior percentage chars.
        #         Match is valid only if an even number of backslash chars
        #         and an even number of percent chars.
    _C_PRINTF_SPECIFIER_REGEX_STR = r'%(?P<flags>[0 #+-]?)(?P<width>(\*|[0-9]*))((\.)(?P<precision>(\*|[0-9]*)))?(?P<data_type>([hl]{0,2}|[jztL])?[diuoxXeEfFgGaAcpsSn])'

    # Finds strings from the .h file to turn back into JSON:
        # e.g., for lines of the form:
        #      [T_FOO]="Bar",
        #      [T_FOO]    =     "Bar", /* comment */
        #      [T_FOO]  ="Bar", // comment
        # matches[0] = ('T_FOO', 'Bar')
        #
        #
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
    _C_EXTRACT_FROM_HEADER_STR = r'\[([a-zA-Z_][a-zA-Z0-9_]*)\]\s*=\s*\"(.*?)\"'

    _CIdentifiers              = None
    _NonTranslatedIdentifiers  = None
    _ExtractFromHeader         = None
    _PrintfSpecifiers          = None

    def __init__(self):
        if self.__class__._CIdentifiers is None:
            self.__class__._CIdentifiers = re.compile(self.__class__._C_IDENTIFIER_REGEX_STR)
        if self.__class__._NonTranslatedIdentifiers is None:
            self.__class__._NonTranslatedIdentifiers = re.compile(self.__class__._C_NON_TRANSLATED_IDENTIFIERS)
        if self.__class__._ExtractFromHeader is None:
            self.__class__._ExtractFromHeader = re.compile(self.__class__._C_EXTRACT_FROM_HEADER_STR)
        if self.__class__._PrintfSpecifiers is None:
            self.__class__._PrintfSpecifiers = re.compile(self.__class__._C_PRINTF_SPECIFIER_REGEX_STR)

    @property
    def CIdentifiers(self):
        return self.__class__._CIdentifiers
    @property
    def NonTranslatedIdentifiers(self):
        return self.__class__._NonTranslatedIdentifiers
    @property
    def ExtractFromHeader(self):
        return self.__class__._ExtractFromHeader
    @property
    def PrintfSpecifiers(self):
        return self.__class__._PrintfSpecifiers

my_regex = MyRegex()



# N.B. - Until Python 3.7, a normal dictionary does not keep insertion order.
#        Require Python 3.7+ explicitly, or enums will randomly shift between builds.
#        or, use `from collections import OrderedDict` to keep insertion order.
#        This would cause random changes in the `enum` order in the generated .h files.
if sys.version_info[0] < 3 or sys.version_info[0] == 3 and sys.version_info[1] < 7:
    print("This script requires Python 3.7 or later.")
    sys.exit(1)

# get the script directory, and make other paths relative
# this allows the script to be called from anywhere and
# still place the files in the correct location.
class MyDirs:
    Scripts          = os.path.dirname(os.path.abspath(__file__))
    Old              = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'old')
    New              = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'templates')
    History          = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'history')
    Templates        = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'templates')
    GeneratedHeaders = os.path.dirname(os.path.abspath(__file__))
print(f"{'Script':>13}: {MyDirs.Scripts}")
print(f"{'History':>13}: {MyDirs.History}")
print(f"{'Templates':>13}: {MyDirs.Templates}")
print(f"{'Generated':>13}: {MyDirs.GeneratedHeaders}")




def get_dictionary_of_key_value_pairs_from_header(file_path):
    key_value_pairs = {}
    with open(file_path, 'r', encoding="utf8") as file:
        lines = file.readlines()
    for line in lines:
        matches = my_regex.ExtractFromHeader.findall(line)
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
        json.dump(translation_dictionary, json_file, indent=4, ensure_ascii=False)

def write_keys_into_enum_header_template(template_file_path, header_file_path, translation_dictionary):

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
    with open(template_file_path, 'r', encoding="utf8") as file:
        content = file.read()

    # Replace the tag with the value
    # Yay for memory-inefficient string manipulations!
    content_with_replacement = content.replace("%%%enum_list%%%", enum_list_string)

    # Write base.h
    with open(header_file_path, 'w', encoding='utf-8') as file:
        # Write the text to the file
        file.write(content_with_replacement)

class DebugJsonConversion(Flag):
    NONE = 0
    NULL_ENTRY = auto()
    UNTRANSLATED = auto()
    IDENTICAL = auto()
    FORMAT_STRINGS = auto()
    OUTDATED = auto()
    ALL = NULL_ENTRY | UNTRANSLATED | IDENTICAL | FORMAT_STRINGS | OUTDATED
    DEFAULT = IDENTICAL | FORMAT_STRINGS | OUTDATED

def convert_remaining_translations_to_h_files(json_directory_path, header_directory_path, base_translation, debug=DebugJsonConversion.DEFAULT):
    if not isinstance(base_translation, dict):
        raise TypeError("base_translation must be a dictionary")
    # and lots of other things, but the above should catch some/most issues

    for filename in os.listdir(json_directory_path):
        if filename.endswith('.json') and filename != 'en-us.json':
            infile_path = os.path.join(json_directory_path, filename)
            with open(infile_path, 'r', encoding="utf8") as json_file:
                target_translation = json.load(json_file)
                target_translation = CanonicalizeEntireTranslation(target_translation)

                output_translation={}
                debug_null = []
                debug_untranslated = []
                debug_identical = []
                debug_mismatched_format_specifiers = []
                debug_outdated_translation = []
                

                file_name_without_extension = os.path.splitext(filename)[0]
                variable_name = file_name_without_extension.replace("-", "_")

                # verify the variable name is a valid identifier in C
                if not my_regex.CIdentifiers.findall(variable_name):
                    raise ValueError(f"ERROR: `{variable_name}` is not a valid C identifier")

                # check if any keys exist in the translation, but NOT in base ... these are VERY likely errors
                for k in target_translation:
                    if k not in base_translation:
                        print(f"WARNING: {file_name_without_extension}: Key `{k}` exists only in the translation.")

                # Verify the format specifiers 

                # Loop through the base translations's keys
                for k in base_translation:
                    base_obj = base_translation[k]
                    if not k in target_translation:
                        debug_untranslated.append(k)
                        output_translation[k] = None
                        continue
                    tgt_obj = target_translation[k]
                    if tgt_obj is None:
                        debug_untranslated.append(k)
                        output_translation[k] = None
                        continue
                    if tgt_obj['Localized'] is None:
                        # No object whatsoever in the json means the string was may not have
                        # existed when that language was translated previously.
                        debug_null.append(k)
                        output_translation[k] = None
                        continue
                    if base_obj['Localized'] == tgt_obj['Localized']:
                        # The translation includes a key with a translated string that is identical to the base (en-us) string.
                        # This indicates the EN-US base string was reviewed, and it was determined no translation is needed.
                        # Header file can store null ... which will revert to the base en-us string in the firmware.
                        debug_identical.append(k)
                        output_translation[k] = None
                        continue
                    # There is a different string stored than existed
                    # validate the same data types are used
                    diffs = describe_format_specifier_differences_between_translation_records(base_obj, tgt_obj)
                    if diffs is not None:
                        # The format specifiers differ ... which makes it unsafe to use
                        # the translated string.  Use NULL in the generated header, which
                        # will revert to the base en-us string in the firmware.
                        debug_mismatched_format_specifiers.append(k)
                        output_translation[k] = None
                        continue
                    # The string the translation was based on differs from current base en-us string
                    if tgt_obj['EN_US'] is None or base_obj['Localized'] != tgt_obj['EN_US']:
                        debug_outdated_translation.append(k)
                        # Probably safe to allow the old translation, even though the
                        # en-us string has changed.
                        # output_translation[k] = null
                        # continue
                    # do not translate certain strings
                    if my_regex.NonTranslatedIdentifiers.match(k):
                        # Certain strings, specifically those used to select a language, should never be translated.
                        # These strings in the base language are either multi-lingual, or they are already in the
                        # language they represent (and thus should not be translated).
                        # Enforcing this here prevents accidentally translating these language selection strings.
                        print(f"  {file_name_without_extension}: Key `{k}` should not be translated from `{base_obj['Localized']}` to `{tgt_obj['Localized']}`.")
                        output_translation[k] = None
                        continue
                    # Only when all the checks succeed, then place the translation into the object
                    output_translation[k] = tgt_obj['Localized']

                # Generate the replacement text for the translated text
                translated_h=""
                for key in output_translation:
                    if output_translation[key] is None:
                        translated_h += f"    [ {key:<32} ] = NULL,\n"
                    else:
                        translated_h += f"    [ {key:<32} ] = \"{output_translation[key]}\",\n"

                # Read the content of the template file
                template_file_path = os.path.join(MyDirs.Templates, 'translation.ht')
                with open(template_file_path, 'r', encoding="utf8") as template_file:
                    content = template_file.read()

                # Replace the tags with the generated values
                content_with_replacement = content.replace("%%%array_data%%%", translated_h)    
                content_with_replacement = content_with_replacement.replace("%%%variable_name%%%", variable_name)

                # Write translation .h file
                output_file_path = os.path.join(header_directory_path, file_name_without_extension + '.h')
                with open(output_file_path, 'w', encoding='utf-8') as output_file:
                    # Write the text to the file
                    output_file.write(content_with_replacement)

                # Print additional debug information if requested
                if debug != DebugJsonConversion.NONE:
                    if len(debug_null) and DebugJsonConversion.NULL_ENTRY in debug:
                        print("\n=============================================================================")
                        print(f"  {file_name_without_extension}: {len(debug_null)} strings are explicitly not translated:")
                        for k in debug_null:
                            print(f"    [ {k:<32} ] = \"{base_translation[k]['Localized']}\",")
                        print("")

                    if len(debug_identical) and DebugJsonConversion.IDENTICAL in debug:
                        print("\n=============================================================================")
                        print(f"  {file_name_without_extension}: {len(debug_identical)} strings are identical to the en-us string.")
                        print(f"  Recommend to either: (a) replace with null in .json to indicate choice to not translate it, or")
                        print(f"  (b) remove the entry entirely to indicate the string has not been translated (yet).")
                        for i in range(0, min(len(debug_identical), 8)):
                            k = debug_identical[i]
                            print(f"    [ {k:<32} ] = \"{base_translation[k]['Localized']}\",")
                        if len(debug_identical) > 8:
                            print("    .... (additional entries not shown) ....")
                        print("")

                    if len(debug_untranslated) and DebugJsonConversion.UNTRANSLATED in debug:
                        print("\n=============================================================================")
                        print(f"  {file_name_without_extension}: {len(debug_untranslated)} strings have no entry in the json.")
                        print(f"  These strings likely still require translation.  If intentionally not translating them,")
                        print(f"  add the entry (with the EN_US entry populated), with `Localized` set to json keyword `null`.")
                        for i in range(0, min(len(debug_untranslated), 8)):
                            k = debug_untranslated[i]
                            if my_regex.NonTranslatedIdentifiers.match(k):
                                print(f"    [ {k:<32} ] = null,")
                            else:
                                print(f"    [ {k:<32} ] = {{ 'Localized':null, 'EN_US':'{base_translation[k]['Localized']}', 'DataTypes':'{base_translation[k]['DataTypes']}' }},")
                        if len(debug_untranslated) > 8:
                            print("    .... (additional entries not shown) ....")
                        print("")

                    if len(debug_outdated_translation) and DebugJsonConversion.OUTDATED in debug:
                        print("\n=============================================================================")
                        print(f"  {file_name_without_extension}: {len(debug_outdated_translation)} strings have outdated translations.")
                        print(f"  These strings have a different (or missing) `EN_US` field, indicating the translated string")
                        print(f"  may be out of date.  The translated string should be reviewed up the `EN_US` field updated")
                        print(f"  to show what en-us string the translation was based on.")
                        for i in range(0, min(len(debug_outdated_translation), 8)):
                            k = debug_outdated_translation[i]
                            print(f"    [ {k:<32} ] = {{ 'Localized':'{target_translation[k]['Localized']}', 'EN_US':'{base_translation[k]['Localized']}', 'DataTypes':{base_translation[k]['DataTypes']} }},")
                        if len(debug_outdated_translation) > 8:
                            print("    .... (additional entries not shown) ....")
                        print("")

                    if len(debug_mismatched_format_specifiers) and DebugJsonConversion.FORMAT_STRINGS in debug:
                        print("\n=============================================================================")
                        print(f"  {file_name_without_extension}: {len(debug_mismatched_format_specifiers)} strings have mismatched format specifiers.")
                        for k in debug_mismatched_format_specifiers:
                            print(f"    [ {k:<32} ] = \"{base_translation[k]['Localized']}\" vs. \"{target_translation[k]['Localized']}\"")
                        print("")
    return

class format_specifiers_iterable:
    def __init__(self, format_string):
        self._format_string = format_string
        self._additional_ints = 0

    def _debug_(self, *args, **kwargs):
        # print(*args, **kwargs)
        None
        
    def __iter__(self):
        self._additional_ints = 0
        self._after_additional_ints = None
        self._re_iter = my_regex.PrintfSpecifiers.finditer(self._format_string)
        return self

    def __next__(self):
        while True:
            if self._additional_ints > 0:
                self._additional_ints -= 1
                return "*"
            if self._after_additional_ints is not None:
                tmp = self._after_additional_ints
                self._after_additional_ints = None
                return tmp

            m = next(self._re_iter)
            if m is StopIteration:
                return StopIteration
            # groups: flags, width, precision, data_type
            self._debug_(f"Potential match @ {m.start(0)} = {m}, data_type = {m.group('data_type')}, flags = {m.group('flags')}, width = {m.group('width')}, precision = {m.group('precision')}")

            # Count the number of preceding percents
            preceding_percents = 0
            for i in range(m.start() - 1, -1, -1):
                if self._format_string[i] == '%':
                    preceding_percents += 1
                else:
                    break
            # Count the number of preceding backslash characters
            preceding_backslashes = 0
            for i in range(m.start() - 1, -1, -1):
                if self._format_string[i] == '\\':
                    preceding_backslashes += 1
                else:
                    break

            if preceding_percents % 2 == 1:
                self._debug_(f"    --> Ignoring match due to odd number of preceding percents")
                continue
            if preceding_backslashes % 2 == 1:
                self._debug_(f"    --> Ignoring match due to odd number of preceding backslashes")
                continue
            
            # check for need to add additional int arguments
            if m.group('width') == '*':
                self._additional_ints += 1
            if m.group('precision') == '*':
                self._additional_ints += 1
            if self._additional_ints > 0:
                self._debug_(f"    --> Adding {self._additional_ints} int parameters for width/precision for type {m.group('data_type')}")
                self._additional_ints -= 1
                self._after_additional_ints = m.group('data_type')
                return "*"

            # everything else is fine, return the data type
            self._debug_(f"    == SUCCESS!")
            return m.group('data_type')

def do_format_specifiers_use_same_datatype(specifier1, specifier2):
    if not isinstance(specifier1, str):
        raise TypeError("specifier1 must be a string")
    if not isinstance(specifier2, str):
        raise TypeError("specifier2 must be a string")
    if specifier1 not in C_FORMAT_SPECIFIERS_TO_DATA_TYPE:
        raise ValueError(f"specifier1 '{specifier1}' is not a valid C printf format specifier")
    if specifier2 not in C_FORMAT_SPECIFIERS_TO_DATA_TYPE:
        raise ValueError(f"specifier2 '{specifier2}' is not a valid C printf format specifier")

    # Which of those types are equivalent for RP2040 / RP2350?
    dt1 = C_FORMAT_SPECIFIERS_TO_DATA_TYPE[specifier1]
    dt2 = C_FORMAT_SPECIFIERS_TO_DATA_TYPE[specifier2]
    return dt1 == dt2

def is_new_format_specifier_compatible_with_prior_format_specifier(old, new):
    if do_format_specifiers_use_same_datatype(old, new):
        return True
    # TODO: check if the data type of one specifier is compatible with the other.
        # Allow new specifier to be a larger integral type than the old specifier
        # e.g., char -> int -> long -> longlong
        # e.g., uchar -> uint -> ulong -> ulonglong
        # e.g., double -> ldouble
    return False

def format_specifier_self_test():
    # These are all the format specifier + length modifier combinations.
    # Can be combined with flags, width, and/or precision options.
    TEST_BASE_C_FORMAT_SPECIFIERS = (
        '%c', '%lc',
        '%s', '%ls',
        '%hhd', '%hd', '%d', '%ld', '%lld', '%jd', '%zd', '%td',
        '%hhi', '%hi', '%i', '%li', '%lli', '%ji', '%zi', '%ti',
        '%hho', '%ho', '%o', '%lo', '%llo', '%jo', '%zo', '%to',
        '%hhx', '%hx', '%x', '%lx', '%llx', '%jx', '%zx', '%tx',
        '%hhX', '%hX', '%X', '%lX', '%llX', '%jX', '%zX', '%tX',
        '%hhu', '%hu', '%u', '%lu', '%llu', '%ju', '%zu', '%tu',
        '%f', '%lf', '%Lf',
        '%F', '%lF', '%LF',
        '%e', '%le', '%Le',
        '%E', '%lE', '%LE',
        '%g', '%lg', '%lG',
        '%G', '%lG', '%LG',
        '%a', '%la', '%La',
        '%A', '%lA', '%LA',
        '%p',
    )
    any_failure = False
    for specifier in TEST_BASE_C_FORMAT_SPECIFIERS:
        result = tuple(format_specifiers_iterable(specifier))
        if len(result) != 1:
            print(f"ERROR: {specifier} -> {result}")
        elif "%" + result[0] != specifier:
            any_failure = True
            print(f"ERROR: {specifier} -> {result}")

    format_string = "%%%%f %08.*s %%%i %8.3d %s %p %99999999.88888Lf %%p %*.*f % A"
    expected_results = ('*', 's', 'i', 'd', 's', 'p', 'Lf', '*', '*', 'f', 'A')
    iterable = format_specifiers_iterable(format_string)
    results = tuple(iterable)
    print(f"Expected results: {expected_results}")
    print(f"  Actual results: {results}")

    if not any_failure:
        print("Base C format specifiers self-test passed.")
    return not any_failure

def get_dictionary_of_format_specifiers(translation_dictionary):
    results = {}
    if not isinstance(translation_dictionary, dict):
        raise TypeError("base_translation must be a dictionary")

    for key in translation_dictionary:
        if not isinstance(key, str):
            raise TypeError(f"each key in dictionary must be a string")
        format_string = translation_dictionary[key]
        if not isinstance(format_string, str):
            raise TypeError(f"base_translation[{key}] must be a string")
        iterable = format_specifiers_iterable(format_string)
        format_specifiers = tuple(iterable)
        if len(format_specifiers) > 0:
            print(f"    {key:<40} -> {format_specifiers}")
        results[key] = format_specifiers

    return results

def read_json_file(file_path):
    result = None
    with open(file_path, 'r', encoding="utf8") as json_file:
        result = json.load(json_file)
    return result


# Get the file path from command line arguments
#if len(sys.argv) < 2:
#    print("Usage: python script.py <us-en.h>")
#    sys.exit(1)

format_specifier_self_test()
#sys.exit(0)


print(f"\n======================================================================")
old_base_translation_path = os.path.join(MyDirs.History, 'en-us.json')
print(f"Reading historical en-us strings from `{old_base_translation_path}`")
old_base_translation = read_json_file(old_base_translation_path)

print(f"\n======================================================================")
base_translation_header_path = os.path.join(MyDirs.GeneratedHeaders, 'en-us.h')
print(f"Extracting base translation key-value pairs from `{base_translation_header_path}`")
base_translation = get_dictionary_of_key_value_pairs_from_header(base_translation_header_path)
for key in base_translation:
    base_translation[key] = CanonicalizeTranslationRecord(base_translation[key])
    base_translation[key]['EN_US'] = base_translation[key]['Localized']
    base_translation[key]['Comments'] = 'Autogenerated from en-us.h'
    base_translation[key]['DataTypes'] = list(format_specifiers_iterable(base_translation[key]['Localized']))

# Note that any change to a string will invalidate prior translations of that string
print(f"\n======================================================================")
print(f"Generating `base.h` from `base.ht` with all enumeration keys")
template = os.path.join(MyDirs.Templates, 'base.ht')
outfile = os.path.join(MyDirs.GeneratedHeaders, 'base.h')
write_keys_into_enum_header_template(template, outfile, base_translation)

print(f"\n======================================================================")
print(f"Temporarily restricting changes to en-us strings (could mess with translations)")

diff_format_specifiers = differences_between_translation_format_specifiers(old_base_translation, base_translation)
if diff_format_specifiers is not None:
    print(f"WARNING: Format specifiers for existing string have changed!")
    new_history_json_path = os.path.join(MyDirs.History, 'new__en-us.json')
    write_key_value_pairs_to_json_file(new_history_json_path, base_translation)
    print(diff_format_specifiers)
    sys.exit(1)

# Note: when lifting this restriction, instead check that format specifiers
#       are unchanged (or at least compatible).
diff_all = differences_between_translations(old_base_translation, base_translation)
if diff_all is not None:
    print(f"WARNING: `en-us.h` does not match `en-us.json` -- Exiting to prevent accidental divergence of translation.")
    print(diff_all)
    new_history_json_path = os.path.join(MyDirs.History, 'new__en-us.json')
    write_key_value_pairs_to_json_file(new_history_json_path, base_translation)
    # After review, if no substantive changes are made to the string, check in the new `en-us.json`
    # by commenting out this check, running the script, reviewing the changes to `en-us.json`, and
    # if they are acceptable, revert changes to the script, and commit the changes to `en-us.json`.
    sys.exit(1)
print(f"   No changes to en-us strings detected.")
# NOTE: Uniquely identifying the string that was translated by ...
#       storing the source string that was translated with each translated string.
#       **ANY** change to the base (en-us) string will cause:
#       1. Warning to be printed here about an outdated translation
#       2. If the format specifiers differ, the generated header for the
#          language will IGNORE the entry, to ensure stability of the firmware.
#       In all cases, the JSON source file will be left unchanged, so that
#       corrective action can be taken separately.

print(f"\n======================================================================")
print(f"Parsing remaining json translations into corresponding .h files")
convert_remaining_translations_to_h_files(MyDirs.Templates, MyDirs.GeneratedHeaders, base_translation, DebugJsonConversion.ALL)
