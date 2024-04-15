# Example of a macro file. 
# Lines starting with '#' are comments 
# Lines starting with '#!' are usage instructions and are printed when 
# requesting macro id 0 
# Every macro line is composed of an an id (>0), a separator ':' and a 
# macro in bus syntax 

#! Get status 
1:{ 0:125 0xc0 } 

#! ClearTrqStatus 
2: {d:125 0x00 0x00 }

#! setPacketType GFSK 
3: {d:125 0x00 0x00 }

#! setRfFrequency 2.4GHz 
4:{ d:125 0x86 0xb8 0x9d 0x89} 
