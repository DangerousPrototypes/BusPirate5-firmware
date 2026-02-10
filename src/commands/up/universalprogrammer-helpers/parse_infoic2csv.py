import xml.etree.ElementTree as ET
import re

tree = ET.parse('infoic.xml')
root = tree.getroot()


print(f'"dbname","manufacturer","icname","ictype","protocol_id","variant","read_buffer_size","write_buffer_size","code_memory_size","data_memory_size","data_memory2_size","page_size","chip_id","voltages","pulse_delay","flags","chip_info","package_details","config"')


for child in root:
    #print(child.tag, child.attrib)
    
    if(child.tag=="database"):
        dbname=child.attrib['type']
        
        for child1 in child:
            #print(dbname, child1.tag, child1.attrib)
            if(child1.tag=="manufacturer"):
                manufacturer=child1.attrib['name']
                
                for child2 in child1:
                    #print(dbname, child2.tag, child2.attrib)
                
                    icname=child2.attrib['name']
                    ictype=child2.attrib['type']
                    protocol_id=child2.attrib['protocol_id']
                    variant=child2.attrib['variant']
                    read_buffer_size=child2.attrib['read_buffer_size']
                    write_buffer_size=child2.attrib['write_buffer_size']
                    code_memory_size=child2.attrib['code_memory_size']
                    data_memory_size=child2.attrib['data_memory_size']
                    data_memory2_size=child2.attrib['data_memory2_size']
                    page_size=child2.attrib['page_size']
                    chip_id=child2.attrib['chip_id']
                    voltages=child2.attrib['voltages']
                    pulse_delay=child2.attrib['pulse_delay']
                    flags=child2.attrib['flags']
                    chip_info=child2.attrib['chip_info']
                    package_details=child2.attrib['package_details']
                    config=child2.attrib['config']
                    
                    print(f'"{dbname}","{manufacturer}","{icname}","{ictype}","{protocol_id}","{variant}","{read_buffer_size}","{write_buffer_size}","{code_memory_size}","{data_memory_size}","{data_memory2_size}","{page_size}","{chip_id}","{voltages}","{pulse_delay}","{flags}","{chip_info}","{package_details}","{config}"')

#INFOIC ic {'name': '628512', 'type': '4', 'protocol_id': '0xd2', 'variant': '0x01', 'read_buffer_size': '0x80', 'write_buffer_size': '0x20', 'code_memory_size': '0x80000', 'data_memory_size': '0x00', 'data_memory2_size': '0x00', 'page_size': '0x0000', 'chip_id': '0x00000000', 'voltages': '0x0000', 'pulse_delay': '0x0000', 'flags': '0x00000080', 'chip_info': '0x0000', 'package_details': '0x20000000', 'config': 'NULL'}

