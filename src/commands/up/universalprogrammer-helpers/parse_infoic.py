import xml.etree.ElementTree as ET
import re

tree = ET.parse('infoic.xml')
root = tree.getroot()


#print(f'"dbname","manufacturer","icname","ictype","protocol_id","variant","read_buffer_size","write_buffer_size","code_memory_size","data_memory_size","data_memory2_size","page_size","chip_id","voltages","pulse_delay","flags","chip_info","package_details","config"')

## 0x02 wasn't documented, assuming 0v?
## 0x06 wasn't documented, assuming 5v from datasheets

#vccs={ 0x00: "5", 0x01: "4", 0x02:"3.3", 0x03: "6.5", 0x04: "5.5", 0x05: "4.5" }
#vpps={ 0x00: "12.5", 0x10: "16", 0x20: "21", 0x30:"13.5", 0x40: "10.0",  0x50: "14", 0x60: "18", 0x70: "17" }
vccs={ 0x00: "UP_VOLT_0500", 0x01: "UP_VOLT_0400", 0x02:"UP_VOLT_0325", 0x03: "UP_VOLT_0650", 0x04: "UP_VOLT_0550", 0x05: "UP_VOLT_0450", 0x06: "UP_VOLT_0500" }
vpps={ 0x00: "UP_VOLT_1250", 0x10: "UP_VOLT_1600", 0x20: "UP_VOLT_2100", 0x30:"UP_VOLT_1350", 0x40: "UP_VOLT_1000", 0x50: "UP_VOLT_1400", 0x60: "UP_VOLT_1800", 0x70: "UP_VOLT_1700", 0x02: "UP_VOLT_0000" }

manufacturers="char manufacturers [][24]={\r\n";
manufacturerid=0;

print("const up_device up_devices[] = {");

for child in root:
    #print(child.tag, child.attrib)
    
    if(child.tag=="database"):
        dbname=child.attrib['type']
        
        for child1 in child:
            #print(dbname, child1.tag, child1.attrib)
            if(child1.tag=="manufacturer"):
                manufacturer=child1.attrib['name']
                
                manufacturers+=" \""+manufacturer+"\", \r\n"
                manufacturerid+=1;
                
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
                    
                    ## old tl866
                    #protocol_id=0x01 i2c eeprom
                    #protocol_id=0x02 93c eeprom
                    #protocol_id=0x03 spi eeprom
                    #protocol_id=0x30 dip32  29xxx
                    #protocol_id=0x31 DIP28, 27xxx
                    #protocol_id=0x32 dip32/plcc32, 27ccc
                    #protocol_id=0x38 dip24/ 27xx
                    #protocol_id=0x39 dip40 27c1024 16 bit
                    
                    if((dbname=="INFOIC")&(ictype=="1")&((protocol_id=="0x31")|(protocol_id=="0x32"))):
                        #print("{"+f'"{manufacturer}", "{icname}", "{chip_id}", "{chip_id}", ??, UP_TYPE_27XXX, {voltages}, {voltages}, {pulse_delay}/4, 10,'+"}" )
                        
                        ## voltages
                        volt=int(voltages, 16)
                        try:
                            vcc=vccs[(volt>>12)&0x0f];
                        except KeyError:
                            vcc=f'{(volt>>12)&0x0f} !!'
                            
                        try:
                            vdd=vccs[(volt>>8)&0x0f];
                        except KeyError:
                            vdd=f'{(volt>>8)&0x0f} !!'
                            
                        try:
                            vpp=vpps[(volt&0x00ff)];
                        except KeyError:
                            vpp=f'{(volt&0x00ff)} !!'
                            
                        ## id
                        id=int(chip_id,16);
                        id1=(id>>8)&0x0FF;   # manufacturer id
                        id2=(id&0x0FF);        # device id
                        
                        ## package
                        pins=(int(package_details, 16))>>24
                        if(pins==0x38):             # plc20
                            pins=20;
                        if(pins==0x3D):             # plc44
                            pins=44;
                        if(pins==0x3e):             # plc28
                            pins=28;
                        if(pins==0x3f):             # plc32
                            pins=32;
                            
                        # pulse
                        pulse=int(pulse_delay, 16)
                        
                        #print(f'"{manufacturer}" "{icname}" {vcc} {vdd} {vpp} 0x{id1:02X} 0x{id2:02X} {package_details} {pins:d} {protocol_id}={variant}');

                        if(pins==0):            # tssop variants
                            continue

                        names=re.split(",",icname)
                        for name in names:
                            if("@" in name):
                                (nam,package)=re.split("@", name)
                                print(" { "+f'"{nam}", {manufacturerid}, 0x{id1:02X}, 0x{id2:02X}, {pins:d}, UP_TYPE_27XXX, {vcc}, {vdd}, {vpp}, {pulse}/4, 25, {code_memory_size}'+"},  //"+f'{protocol_id}={variant} package {package}')

                            else:
                                print(" { "+f'"{name}", {manufacturerid}, 0x{id1:02X}, 0x{id2:02X}, {pins:d}, UP_TYPE_27XXX, {vcc}, {vdd}, {vpp}, {pulse}/4, 25, {code_memory_size}'+"},  //"+f'{protocol_id}={variant}')

    
    








                    #print(f'"{dbname}","{manufacturer}","{icname}","{ictype}","{protocol_id}","{variant}","{read_buffer_size}","{write_buffer_size}", "{code_memory_size}","{data_memory_size}","{data_memory2_size}","{page_size}","{chip_id}","{voltages}","{pulse_delay}","{flags}","{chip_info}","{package_details}","{config}"')

#//  123456789012345  id1    id2   pins type           kbit          Vdd         Vpp          t(us)  retries

#INFOIC ic {'name': '628512', 'type': '4', 'protocol_id': '0xd2', 'variant': '0x01', 'read_buffer_size': '0x80', 'write_buffer_size': '0x20', 'code_memory_size': '0x80000', 'data_memory_size': '0x00', 'data_memory2_size': '0x00', 'page_size': '0x0000', 'chip_id': '0x00000000', 'voltages': '0x0000', 'pulse_delay': '0x0000', 'flags': '0x00000080', 'chip_info': '0x0000', 'package_details': '0x20000000', 'config': 'NULL'}


print("};")
manufacturers+="};\r\n"
print(manufacturers);

