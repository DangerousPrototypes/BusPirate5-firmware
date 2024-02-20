# A python script for converting Adafruit I2C list markdown files into optimized C arrays
# Public Domain/CC0 Ian Lesnet for Bus Pirate 5, 2024
import json
import sys
import re
import os

def get_key_by_value(my_dict, search_value):
    for key, value in my_dict.items():
        if value == search_value:
            return key
    return None  # Return None if the value is not found in the dictionary

# Parse markdown list to create dictionary with I2C addresses as keys and lists of device names as values
device_dict = {}
current_address = None
file_names=['0x00-0x0F.md','0x10-0x1F.md','0x20-0x2F.md','0x30-0x3F.md','0x40-0x4F.md','0x50-0x5F.md','0x60-0x6F.md','0x70-0x7F.md']
for file_name in file_names:
    with open(file_name, 'r', encoding="utf8") as file:
        lines = file.readlines()
    for line in lines:
        if line.startswith('##'):
            current_address = int(re.findall(r'## (0x[0-9A-Fa-f]+)', line)[0], 16)
            device_dict[current_address] = []
        elif line.startswith('-'):
            if current_address is not None:
                if ']' in line:
                    device_name = line.split('[')[1].split(']')[0]
                else:
                    device_name = line.split('- ')[1].split('(')[0].strip()
                #print(device_name)
                device_dict[current_address].append(device_name)

print("Device Dictionary:")
#print(device_dict)


# Join the array of device descriptions into a string
# New dictionary for consolidated text elements
consolidated_device_dict = {}

# Iterate through the original dictionary
for address, devices_list in device_dict.items():
    # Join the list of devices into a single string
    consolidated_text = ', '.join(devices_list)
    # Assign the consolidated text to the address key in the new dictionary
    consolidated_device_dict[address] = consolidated_text

#print(consolidated_device_dict)
# Print the consolidated device dictionary
#for address, consolidated_text in consolidated_device_dict.items():
#    print(f"{address}: {consolidated_text}")

lookup = {}
base = {}
char_arr={}
idx=0
for i in range(0, 128):
    if i in consolidated_device_dict:
        idx_used=get_key_by_value(char_arr, consolidated_device_dict[i])
        if idx_used is not None:
            base[i]=idx_used
        else:
            char_arr[idx]=consolidated_device_dict[i]      
            base[i]=idx
            char_arr[idx]=consolidated_device_dict[i]
            idx=idx+1
    else:
        base[i]=None



#c_char_array = "[" + ", ".join(output) + "]"
#print(c_char_array)

#print(base)
#quit()
output2 = []
for id, device_str_idx in char_arr.items():
    if device_str_idx is not None:
        output2.append(f"DEV_I2C_LIST_{id}")
 

#c_char_array = "[" + ", ".join(output) + "\n]"
#print(c_char_array)

# Create base.h file with all the translation keys (defines)
base_h=""

# Iterate over the keys in base_translation
for index, key in enumerate(output2):
        base_h += "\t" + key + ",\n"

output3 = []
for id, device_str_idx in base.items():
    if device_str_idx is not None:
        output3.append(f"DEV_I2C_LIST_{device_str_idx}")
    else:
        output3.append(f"DEV_I2C_LIST_NONE")   

ptr_h=""
for index, key in enumerate(output3):
        ptr_h += "\t&dev_i2c_addresses_text[" + key + "],\n"

translated_h=""

# Iterate over the keys in output_translation
for id, device_str in char_arr.items():
    translated_h += "\t[DEV_I2C_LIST_" + str(id) + "]=\""+device_str+"\",\n"

# Read the content of the file
with open('dev_i2c_addresses.ht', 'r', encoding="utf8") as file:
    content = file.read()    

# Replace the tag with the value
content = content.replace("%%%enum_list%%%", base_h)    
# Replace the tag with the value
content = content.replace("%%%array_data%%%", translated_h)    
content = content.replace("%%%pointer_array%%%", ptr_h)

# Write translation .h file
with open('dev_i2c_addresses.h', 'w', encoding='utf-8') as output_file:
    # Write the text to the file
    output_file.write(content)
