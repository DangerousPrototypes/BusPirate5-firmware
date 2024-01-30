import json
import sys
import re
import os



# Define the regular expression pattern
pattern = r'\[(.*?)\]="(.*?)"'

# Function to parse key-value pairs from a file
def parse_key_value_pairs(file_path):
    key_value_pairs = {}
    with open(file_path, 'r', encoding="utf8") as file:
        lines = file.readlines()
    for line in lines:
        matches = re.findall(pattern, line)
        if matches:
            key, value = matches[0]
            key_value_pairs[key]=value
        else:
            print("No match found for line:", line.strip())
    return key_value_pairs

# Get the file path from command line arguments
#if len(sys.argv) < 2:
#    print("Usage: python script.py <us-en.h>")
#    sys.exit(1)

# Parse key-value pairs from the first file
base_translation = parse_key_value_pairs('en-us.h')

base_h=""

# Iterate over the keys in base_translation
for index, key in enumerate(base_translation):
    # Check if it's the first iteration
    if index == 0:
        base_h += "\t" + key + "=0,\n"
    else:
        base_h += "\t" + key + ",\n"

# Read the content of the file
with open('base.ht', 'r', encoding="utf8") as file:
    content = file.read()    

# Replace the tag with the value
content_with_replacement = content.replace("%%%enum_list%%%", base_h)    

# Write base.h
with open('base-test.h', 'w', encoding='utf-8') as file:
    # Write the text to the file
    file.write(content_with_replacement)


############################
# Done with base.h update
# Turn each .json into .h
############################
directory='.'
for filename in os.listdir(directory):
    if filename.endswith('.json') and filename != 'en-us.json':
        filepath = os.path.join(directory, filename)
        with open(filepath, 'r', encoding="utf8") as json_file:
            target_translation = json.load(json_file)

            output_translation={}

            file_name_without_extension = os.path.splitext(filename)[0]

            # Parse key-value pairs from the second file
            #target_translation = parse_key_value_pairs(temp_file)


            # Compare key-value pairs between the two files
            for key in base_translation:
                if key in target_translation:
                    #print("Key", key, "exists only in the first file.")
                    output_translation[key]=target_translation[key] #take the localization if it exists
                else:
                    print("Key", key, "exists only in the base translation.")
                    output_translation[key]=base_translation[key] # use base translation if no localization exists

            translated_h=""

            # Iterate over the keys in output_translation
            for key in output_translation:
                translated_h += "\t[" + key + "]=\""+output_translation[key]+"\",\n"

            # Read the content of the template file
            with open('translation.ht', 'r', encoding="utf8") as template_file:
                content = template_file.read()    

            # Replace the tag with the value
            content_with_replacement = content.replace("%%%array_data%%%", translated_h)    
            content_with_replacement = content_with_replacement.replace("%%%variable_name%%%", file_name_without_extension.replace("-", "_"))   

            # Write translation .h file
            with open(file_name_without_extension+'-test.h', 'w', encoding='utf-8') as output_file:
                # Write the text to the file
                output_file.write(content_with_replacement)




# Write master_translation to a JSON file
#with open('zh-cn-test.h', 'w', encoding='utf-8') as json_file:
#    json.dump(master_translation, json_file, indent=4, ensure_ascii=False)

#for pair in file2_key_value_pairs:
#    if pair not in file1_key_value_pairs:
#        print("Pair", pair, "exists only in the second file.")
