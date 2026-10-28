# Open Source Licenses
This file lists all the open source licenses that are being used by this project, and how this project complies with them.  

All the content in this document is copyrighted by the following authors:  
* Copyright (c) 2024 Lior Shalmay <liorshalmay1@gmail.com>  

## Disclaimer
All the source code in [this project](https://github.com/DangerousPrototypes/BusPirate5-firmware) is distributed under 
MIT license (unless specified explicitly otherwise, at the start of the file). This produced does use libraries which are distributed under different licenses, documented in this file.

Any distributed binary derived from this project should be distributed alongside a copy of this document, a copy of all 
the licenses inside the `docs/third_party_licenses` directory and a copy of MIT license.

You may not need to distribute some of the licenses and claims, if you select to not use certain libraries.

### Copyright notices
All the people listed below hold copyrights to code in this project:  
* Copyright (c) 2026 Dawid Konrad Kohnke <dawid.kohnke.cad@gmail.com>
* Copyright (c) 2023-2024 Ian Lesnet, Where Labs LLC 
* Copyright (c) 2024 Lior Shalmay <liorshalmay1@gmail.com>
* Copyright (c) 2019 Ha Thach (tinyusb.org)
* Copyright (c) 2020-2021 Raspberry Pi (Trading) Ltd.
* Copyright (c) 2021 Stefan Althöfer
* Copyright (c) 2013 Daniel Beer <dlbeer@gmail.com>
* Copyright (c) 2023 Paul Campbell, Moonbase Otago  paul@taniwha.com
* Copyright (c) 2019, ChaN, all right reserved.
* Copyright (c) 2022 Kattni Rembor for Adafruit Industries
* Copyright (c) 2014 Kosma Moczek <kosma@cloudyourcar.com>
* Copyright (c) 2016 Measurement Specialties. All rights reserved.
* Copyright (c) 2016-2018, Armink, <armink.ztl@gmail.com>
* Copyright (c) 2015-2021 LibDriver All rights reserved
* Copyright (c) 2018-2020 Cesanta Software Limited
* Copyright (c) 2021 Jaroslav Kysela <perex@perex.cz>
* Copyright (c) 2014-2019 Marco Paland (info@paland.com), PALANDesign Hannover, Germany
* Copyright (c) 2017,2019 Two Blue Cubes Ltd. All rights reserved.
* Copyright (c) 2009 Mikael Gustafsson (ds30 Loader, see the GPL section)


## LGPL3
### Used libraries
#### [ansi_colours](https://github.com/mina86/ansi_colours)
##### Files that are being used from the project
* `ansi256.c`
* `ansi_colours.h`
##### Source Files that use this library
* `ui/ui_term.c`
##### copyright notice
Copyright 2018 by Michał Nazarewicz <mina86@mina86.com>

### License Compliance explained
First of all, we give the copyright notices in all the respective places in this repo.
Secondly, this project is completely open source and published to the public, and distributed binaries are delivered with a link to this repo.
Thirdly, the original libraries code is left unmodified, and their code gets fetched by the build system, therefore that code is not part of this project.

Therefore, we comply with all the terms of the LGPL3 license, including section 4.d.0, because by access to this source code, the user can easily use the build system in order to provide a different library that has the same interface.

In conclusion, these are the reasons that we comply with LGPL3 and we can distribute binaries under LGPL3 terms.  
For further information, please see the discussion at [github](https://github.com/DangerousPrototypes/BusPirate5-firmware/pull/34).

## GPL
### Used programs
#### [ds30 Loader](http://mrmackey.no-ip.org/elektronik/ds30loader/), the Bus Pirate v3 bootloader v4.5
##### Files that are being used from the project
* `lib/ds30loader/`: the source of the bootloader (`ds30loader.s`, `settings.inc`, `devices.inc`)
* `binmode/bp3unbrick_bootloader.h`: the machine code of the bootloader, as data
##### Source Files that use this program
* `binmode/bp3unbrick.c`
##### copyright notice
Copyright (c) 2009 Mikael Gustafsson, modified for the Bus Pirate v2go and v3.
ds30 Loader is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation.

### License Compliance explained
The bootloader is a separate program for another processor, the PIC24FJ64GA002 of a Bus Pirate v3. This firmware stores its machine code as data and writes it to that PIC over ICSP. The two are not linked and the bootloader never runs on the Bus Pirate 5/6/7, so they form an aggregate, and the GPL covers the bootloader only.

Its source code is in `lib/ds30loader/`, and the license text is in `docs/third_party_licenses/gpl-3.0.txt`. The source does not name a GPL version, which allows any version.

## BSD 3 Clause
This license is completely compatible with our MIT license. So BSD components have been directly incorporated into this project.

### List of source files
- `queue.c`
- `queue.h`
- `commands/1wire/`
    - `demos.c`
    - `scan.c`
- `lib/sigrok/pico_sdk_sigrok.c`
- `mode/logicanalyzer.pio`
- `pirate/`
    - `apa102.pio`
    - `hw1wire_pio.c`
    - `hw1wire.pio`
    - `hw2wire_pio.c`
    - `hw2wire_pio.h`
    - `hw2wire.pio`
    _ `hwi2c_pio.c`
    _ `hwi2c_pio.h`
    - `hwi2c.pio`
    - `hwuart.pio`
    - `ws2812.pio`




