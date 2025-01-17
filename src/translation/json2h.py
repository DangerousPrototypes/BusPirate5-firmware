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

C_FORMAT_SPECIFIERS_TO_DATA_TYPE = {
    # TODO: entry should be a tuple, with second and later elements being "acceptable" equivalents (e.g., larger sized, pvoid, etc.
    # See https://en.cppreference.com/w/c/io/fprintf
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

# N.B. - Until Python 3.7, a normal dictionary does not keep insertion order.
#        Require Python 3.7+ explicitly, or enums will randomly shift between builds.
#        or, use `from collections import OrderedDict` to keep insertion order.
#        This would cause random changes in the `enum` order in the generated .h files.
if sys.version_info[0] < 3 or sys.version_info[0] == 3 and sys.version_info[1] < 7:
    print("This script requires Python 3.7 or later.")
    sys.exit(1)



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
    FORMAT_STRINGS = auto()
    ALL = UNTRANSLATED | IDENTICAL | NULL_ENTRY | FORMAT_STRINGS
    DEFAULT = FORMAT_STRINGS | IDENTICAL


def convert_remaining_translations_to_h_files(json_directory, header_directory, debug=DebugJsonConversion.DEFAULT):

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
                mismatched_format_specifiers = []

                file_name_without_extension = os.path.splitext(filename)[0]
                variable_name = file_name_without_extension.replace("-", "_")
                # BUGBUG -- verify it's a valid identifier in C
                if not my_regex.CIdentifiers.findall(variable_name):
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
                        output_translation[key] = None
                        continue

                    tgt_str = target_translation[key]                        
                    if tgt_str is None:
                        # null in the json means the string was previously reviewed for
                        # translation, but an intentional choice was made to NOT translate it.
                        debug_null.append(key)
                        output_translation[key] = None
                        continue
                    if base_translation[key] == tgt_str:
                        # If the translation includes a key with a translated string that is identical to the base (en-us) string:
                        # Technically this is not an error.  However, if the base string changes (e.g., for clarity or to fix a typo),
                        # the translation .json might be missed in the update.
                        # Therefore, it is likely better to remove the key from the .json file, unless wanting to explicitly "lock in"
                        # a translation, even if the en-us string changes.
                        debug_identical.append(key)
                        output_translation[key] = None
                        continue

                    # validate the same correct data types are used
                    data_types = tuple(format_specifiers_iterable(tgt_str))
                    expected_types = current_format_specifiers[key] # global variable ... pooh!
                    if len(data_types) != len(expected_types):
                        mismatched_format_specifiers.append(key)
                        output_translation[key] = None
                        continue
                    tmp_mismatch_specifiers = False
                    for i in range(len(data_types)):
                        if not do_format_specifiers_use_same_datatype(data_types[i], expected_types[i]):
                            tmp_mismatch_specifiers = True
                    if tmp_mismatch_specifiers:
                        mismatched_format_specifiers.append(key)
                        output_translation[key] = None
                        continue

                    # do not translate certain strings
                    if my_regex.NonTranslatedIdentifiers.match(key):
                        # Certain strings, specifically those used to select a language, should never be translated.
                        # These strings in the base language are either multi-lingual, or they are already in the
                        # language they represent (and thus should not be translated).
                        # Enforcing this here prevents accidentally translating these language selection strings.
                        print(f"  {file_name_without_extension}: Key `{key}` should not be translated from `{base_translation[key]}` to `{target_translation[key]}`.")
                        output_translation[key] = None
                        continue

                    # Only when all the checks succeed, then place the translation into the object
                    output_translation[key] = tgt_str

                # Generate the replacement text for the translated text
                translated_h=""
                for key in output_translation:
                    if output_translation[key] is None:
                        translated_h += f"    [ {key:<32} ] = NULL,\n"
                    else:
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
                            if my_regex.NonTranslatedIdentifiers.match(k):
                                print(f"    [ {k:<32} ] = null,")
                            else:
                                print(f"    [ {k:<32} ] = \"{base_translation[k]}\",")
                        print("")
                    
                    if len(mismatched_format_specifiers) and DebugJsonConversion.FORMAT_STRINGS in debug:
                        print("\n=============================================================================")
                        print(f"  {file_name_without_extension}: {len(mismatched_format_specifiers)} strings have mismatched format specifiers.")
                        for k in mismatched_format_specifiers:
                            print(f"    [ {k:<32} ] = \"{base_translation[k]}\" vs. \"{target_translation[k]}\"")
                        print("")


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

    format_string = "%%%%f  \%s %\%i %8.3d %s %p %99999999.88888Lf %%p %*.*f"
    expected_results = ('d', 's', 'p', 'Lf', '*', '*', 'f')
    iterable = format_specifiers_iterable(format_string)
    results = tuple(iterable)
    print(f"Expected results: {expected_results}")
    print(f"Actual results: {results}")

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

def verify_unchanged_translation(old_translation, new_translation):
    if not isinstance(old_translation, dict):
        raise TypeError("old_base_translation must be a dictionary")
    if not isinstance(new_translation, dict):
        raise TypeError("new_translation must be a dictionary")
    # NOTE: It's fine to have extra keys in the new translation
    #       Just want to catch existing strings that are being modified
    for key in old_translation:
        if not isinstance(key, str):
            raise TypeError(f"each key in dictionary must be a string")
        if key not in new_translation:
            print(f"  Key `{key}` is missing from the new translation.")
            return False
        if old_base_translation[key] != base_translation[key]:
            print(f"  Value for key `{key}` has changed from `{old_base_translation[key]}` to `{base_translation[key]}`.")
            return False
    return True

# Get the file path from command line arguments
#if len(sys.argv) < 2:
#    print("Usage: python script.py <us-en.h>")
#    sys.exit(1)

# format_specifier_self_test()
# sys.exit(0)

# Parse key-value pairs from the first file

print(f"\n======================================================================")
print(f"Reading prior version of en-us strings from `en-us.json`")
old_base_translation = read_json_file(os.path.join('.', 'en-us.json'))
print(f"\n======================================================================")
print(f"Obtaining key-value pairs from `en-us.h`")
base_translation = get_dictionary_of_key_value_pairs_from_header('en-us.h')
print(f"\n======================================================================")
print(f"Verifying en-us strings unchanged (could mess with translations)")
if not verify_unchanged_translation(old_base_translation, base_translation):
    print(f"ERROR: `en-us.h` does not match `en-us.json` -- Exiting to prevent accidental divergence of translation.")
    # After review, if no substantive changes are made to the string, check in the new `en-us.json`
    # by commenting out this check, running the script, reviewing the changes to `en-us.json`, and
    # if they are acceptable, revert changes to the script, and commit the changes to `en-us.json`.
    # TODO: lock-down changes to `en-us.json` on github?  add to .gitignore to force it to be intentionally changed?
    # TODO: Maybe a github action to check that `en-us.json` is not changed?
    # TODO: Maybe a github action uses this script to validate stuff?
    sys.exit(1)

print(f"\n======================================================================")
print(f"Detecting format specifiers for strings from `en-us.h`")
current_format_specifiers = get_dictionary_of_format_specifiers(base_translation)

# TODO: How to uniquely identify a source version of en-us, so can track with
#       a translation JSON?  Purpose is to show which en-us strings have changed
#       or been added since the last translation.  Possibly stored on a per-entry
#       basis?  Maybe just store the en-us version of the string it was based on?
#       BUT ... that changes the dictionary to have a structure for each constant,
#       not just a string / null.
#       This is probably the right way to move forward, as it allows storing
#       comments and other metadata also.

print(f"\n======================================================================")
print(f"Generating `format_specifier_history.txt` (a json file)")
write_key_value_pairs_to_json_file('format_specifier_history.txt', current_format_specifiers)


print(f"\n======================================================================")
print(f"Generating `en-us.json` from `en-us.h`")
write_key_value_pairs_to_json_file('en-us.json', base_translation)
print(f"\n======================================================================")
print(f"Generating `base.h` from `base.ht` with all enumeration keys")
write_keys_into_enum_header_template('base.ht', 'base.h', base_translation)
print(f"\n======================================================================")
print(f"Parsing remaining json translations into corresponding .h files")
convert_remaining_translations_to_h_files('.', '.')
# To help with translations, the following debug flags can be used to
# print additional information about entries in the .json files, such
# as missing entries (likely need translation), identical entries (which
# likely should either be set to JSON null, or removed from the .json
# file altogether.
# convert_remaining_translations_to_h_files('.', '.', DebugJsonConversion.DEFAULT)
