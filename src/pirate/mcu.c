#include <stdio.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "pico/unique_id.h"
#include "hardware/watchdog.h"
#include "pico/bootrom.h"

uint64_t mcu_get_unique_id(void) {
    static_assert(sizeof(pico_unique_board_id_t) == sizeof(uint64_t),
                  "pico_unique_board_id_t is not 64 bits (but is cast to uint64_t)");
    pico_unique_board_id_t id;
    pico_get_unique_board_id(&id);
    // NOTE: Treating the serial number as a 64-bit integer,
    //       because the platform is little-endian,
    //       has the unfortunate effect of reversing the bytes
    //       (vs. viewing as a byte array).
    uint64_t result =
        ((uint64_t)id.id[0] << (8*0)) |
        ((uint64_t)id.id[1] << (8*1)) |
        ((uint64_t)id.id[2] << (8*2)) |
        ((uint64_t)id.id[3] << (8*3)) |
        ((uint64_t)id.id[4] << (8*4)) |
        ((uint64_t)id.id[5] << (8*5)) |
        ((uint64_t)id.id[6] << (8*6)) |
        ((uint64_t)id.id[7] << (8*7)) ;
    uint64_t result2 = *((uint64_t*)(id.id)); // ... breaks strict-aliasing rules ...
    assert(result == result2);
    return result;
}

void mcu_reset(void) {
    watchdog_enable(1, 1);
    while (1)
        ;
}

void mcu_jump_to_bootloader(void) {
    /* \param usb_activity_gpio_pin_mask 0 No pins are used as per a cold boot. Otherwise a single bit set indicating
     * which GPIO pin should be set to output and raised whenever there is mass storage activity from the host. \param
     * disable_interface_mask value to control exposed interfaces
     *  - 0 To enable both interfaces (as per a cold boot)
     *  - 1 To disable the USB Mass Storage Interface
     *  - 2 To disable the USB PICOBOOT Interface
     */
    reset_usb_boot(0x00, 0x00);
}

uint8_t mcu_detect_revision(void) {
    gpio_set_function(23, GPIO_FUNC_SIO);
    gpio_set_dir(23, true);
    gpio_put(23, true);
    busy_wait_ms(100);
    gpio_set_dir(23, false);
    busy_wait_us(1);
    if (gpio_get(23)) {
        return 10;
    } else {
        return 8;
    }
}