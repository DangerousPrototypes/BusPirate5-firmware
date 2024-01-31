import json
import sys
import re
import os

master_translation={}

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
if len(sys.argv) < 2:
    print("Usage: python script.py <us-en.h> <other-translation.h>")
    sys.exit(1)

# Parse key-value pairs from the first file
base_translation = parse_key_value_pairs(sys.argv[1])

# Parse key-value pairs from the second file
target_translation = parse_key_value_pairs(sys.argv[2])

# Compare key-value pairs between the two files
for key in base_translation:
    if key in target_translation:
        #print("Key", key, "exists only in the first file.")
        master_translation[key]=target_translation[key] #take the localization if it exists
    #else:
       # print("Key", key, "exists only in the base translation.")
        #master_translation[key]=base_translation[key] # use base translation if no localization exists

# Write master_translation to a JSON file
with open(os.path.splitext(sys.argv[2])[0]+'.json', 'w', encoding='utf-8') as json_file:
    json.dump(master_translation, json_file, indent=4, ensure_ascii=False)

#for pair in file2_key_value_pairs:
#    if pair not in file1_key_value_pairs:
#        print("Pair", pair, "exists only in the second file.")
