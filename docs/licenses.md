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
* Copyright (c) 2025-2026 Chris van Dongen, SMDprutser


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




