TODO: make this a nice and shiny md file

This directory contains the utilities used by the universal programmer to generate various lookup tables and include files. Note these are quick'n'dirty hacks to generate them :D

parse_infoic2csv.py     parses infoic.xml to a csv 
parse_infoic.py         parses infoic.xml to up_eproms.h
parse_logicic.py        parses logic.xml to up_logicic.h

makelut_27xx.c          generates the lookuptable for 27xx eproms
makelut_dram41xx.c      generates the lookuptable for 41xx dram chips

infoic.xml and logic.xml from https://gitlab.com/DavidGriffith/minipro

crc generation from http://www.mrob.com/pub/comp/crc-all.html (makecrctable.c generates arc.c, binhex.c, ccitt32.c, ccitt.c, kermit.c, zip.c) 


