[English](/README.md) | [ 简体中文](/README_zh-Hans.md) | [繁體中文](/README_zh-Hant.md) | [日本語](/README_ja.md) | [Deutsch](/README_de.md) | [한국어](/README_ko.md)

<div align=center>
<img src="/doc/image/logo.svg" width="400" height="150"/>
</div>

## LibDriver TSL2561

[![MISRA](https://img.shields.io/badge/misra-compliant-brightgreen.svg)](/misra/README.md) [![API](https://img.shields.io/badge/api-reference-blue.svg)](https://www.libdriver.com/docs/tsl2561/index.html) [![License](https://img.shields.io/badge/license-MIT-brightgreen.svg)](/LICENSE)

The TSL2560 and TSL2561 are light-to-digital converters that transform light intensity to a digital signal output capable of direct IIC (TSL2561) or SMBus (TSL2560) interface. Each device combines one broadband photodiode (visible plus infrared) and one infrared-responding photodiode on a single CMOS integrated circuit capable of providing a near-photopic response over an effective 20-bit dynamic range (16-bit resolution). Two integrating ADCs convert the photodiode currents to a digital output that represents the irradiance measured on each channel. This digital output can be input to a microprocessor where illuminance (ambient light level) in lux is derived using an empirical formula to approximate the human eye response. The TSL2560 device permits an SMB-Alert style interrupt, and the TSL2561 device supports a traditional level style interrupt that remains asserted until the firmware clears it. While useful for general purpose light sensing applications, the TSL2560/61 devices are designed particularly for display panels(LCD, OLED, etc.) with the purpose of extending battery life and providing optimum viewing in diverse lighting conditions. Display panel backlighting, which can account for up to 30 to 40 percent of total platform power, can be automatically managed. Both devices are also ideal for controlling keyboard illumination based upon ambient lighting conditions. Illuminance information can further be used to manage exposure control in digital cameras. The TSL2560/61 devices are ideal in notebook/tablet PCs, LCD monitors, flat-panel televisions, cell phones, and digital cameras. In addition, other applications include street light control, security lighting, sunlight harvesting, machine vision, and automotive instrumentation clusters.

LibDriver TSL2561 is the TSL2561 full function driver launched by LibDriver.It provides brightness reading, brightness interrupt detection and other functions. LibDriver is MISRA compliant.

### Table of Contents

  - [Instruction](#Instruction)
  - [Install](#Install)
  - [Usage](#Usage)
    - [example basic](#example-basic)
    - [example interrupt](#example-interrupt)
  - [Document](#Document)
  - [Contributing](#Contributing)
  - [License](#License)
  - [Contact Us](#Contact-Us)

### Instruction

/src includes LibDriver TSL2561 source files.

/interface includes LibDriver TSL2561 IIC platform independent template.

/test includes LibDriver TSL2561 driver test code and this code can test the chip necessary function simply.

/example includes LibDriver TSL2561 sample code.

/doc includes LibDriver TSL2561 offline document.

/datasheet includes TSL2561 datasheet.

/project includes the common Linux and MCU development board sample code. All projects use the shell script to debug the driver and the detail instruction can be found in each project's README.md.

/misra includes the LibDriver MISRA code scanning results.

### Install

Reference /interface IIC platform independent template and finish your platform IIC driver.

Add /src, /interface and /example to your project.

### Usage

#### example basic

```C
#include "driver_tsl2561_basic.h"

uint8_t res;
uint8_t i;
uint32_t lux;

res = tsl2561_basic_init(TSL2561_ADDRESS_FLOAT);
if (res != 0)
{
    return 1;
}

...

for (i = 0; i < 3; i++)
{
    tsl2561_interface_delay_ms(1000);
    res = tsl2561_basic_read((uint32_t *)&lux);
    if (res != 0)
    {
        (void)tsl2561_basic_deinit();

        return 1;
    }
    tsl2561_interface_debug_print("tsl2561: read is %d lux.\n", lux);

    ...
    
}

...

(void)tsl2561_basic_deinit();

return 0;
```

#### example interrupt

```C
#include "driver_tsl2561_interrupt.h"

uint8_t res;
uint8_t i;
uint32_t lux;
uint8_t g_flag;

void gpio_irq(void)
{
    g_flag = 1;
}

res = tsl2561_interrupt_init(TSL2561_ADDRESS_FLOAT, TSL2561_INTERRUPT_MODE_EVERY_ADC_CYCLE, 10, 100);
if (res != 0)
{
    return 1;
}
res = gpio_interrupt_init();
if (res != 0)
{
    (void)tsl2561_interrupt_deinit();

    return 1;
}

...

g_flag = 0;
for (i = 0; i < 3; i++)
{
    tsl2561_interface_delay_ms(1000);
    res = tsl2561_interrupt_read((uint32_t *)&lux);
    if (res != 0)
    {
        (void)tsl2561_interrupt_deinit();
        (void)gpio_interrupt_deinit();

        return 1;
    }
    tsl2561_interface_debug_print("tsl2561: %d/%d.\n", (uint32_t)(i+1), (uint32_t)times);
    tsl2561_interface_debug_print("tsl2561: read is %d lux.\n", lux);

    ...
    
    if (g_flag != 0)
    {
        tsl2561_interface_debug_print("tsl2561: find interrupt.\n");

        break;
    }
    
    ...

}

...

(void)tsl2561_interrupt_deinit();
(void)gpio_interrupt_deinit();

return 0;
```

### Document

Online documents: [https://www.libdriver.com/docs/tsl2561/index.html](https://www.libdriver.com/docs/tsl2561/index.html).

Offline documents: /doc/html/index.html.

### Contributing

Please refer to CONTRIBUTING.md.

### License

Copyright (c) 2015 - present LibDriver All rights reserved



The MIT License (MIT) 



Permission is hereby granted, free of charge, to any person obtaining a copy

of this software and associated documentation files (the "Software"), to deal

in the Software without restriction, including without limitation the rights

to use, copy, modify, merge, publish, distribute, sublicense, and/or sell

copies of the Software, and to permit persons to whom the Software is

furnished to do so, subject to the following conditions: 



The above copyright notice and this permission notice shall be included in all

copies or substantial portions of the Software. 



THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR

IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,

FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE

AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER

LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,

OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE

SOFTWARE. 

### Contact Us

Please sent an e-mail to lishifenging@outlook.com.