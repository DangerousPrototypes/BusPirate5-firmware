# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# initalize pico_sdk from installed location
# (note this can come from environment, CMake cache etc)
#set(PICO_SDK_PATH "C:/pico113/pico-sdk")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

set(PICO_BOARD pico)

project(bus_pirate C CXX ASM) 

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

add_compile_definitions(BP_FIRMWARE_HASH=${BP_FIRMWARE_HASH})

# Add executable. Default name is the project name, version 0.1
add_executable(bus_pirate 
        pirate.c pirate.h platform/bpi-rev8.h platform/bpi-rev8.c commands.h commands.c 
        ui/ui_lcd.c ui/ui_lcd.h rgb.c rgb.h bio.h bio.c
        msc_disk.c usb_descriptors.c 
        shift.h shift.c psu.h psu.c amux.h amux.c buttons.c buttons.h
        system_config.h system_config.c modes.c modes.h auxpinfunc.h auxpinfunc.c
        pwm.c pwm.h freq.c freq.h adc.c adc.h pullups.c pullups.h helpers.h helpers.c
        mode/dummy1.c mode/dummy1.h mode/hiz.h mode/hiz.c mode/hwspi.c mode/hwspi.h
        mode/usart.h mode/usart.c mode/hwi2c.c mode/hwi2c.h mode/hwled.c mode/hwled.h
        ui/ui_cmdln.c ui/ui_cmdln.h ui/ui_process.h ui/ui_process.c
        #mode/sw1wire.h mode/sw1wire.c
        ui/ui_flags.h
        debug.c debug.h
        ui/ui_parse.c ui/ui_parse.h ui/ui_prompt.h ui/ui_prompt.c  ui/ui_mode.c ui/ui_mode.h
        ui/ui_info.h ui/ui_info.c ui/ui_format.c ui/ui_format.c ui/ui_init.c ui/ui_init.h
        ui/ui_const.h ui/ui_const.c ui/ui_config.h ui/ui_config.c 
        translation/base.h translation/base.c translation/en-us.h translation/zh-cn.h
        queue.c queue.h 
        #buf.c buf.h
        usb_tx.c usb_tx.h usb_rx.c usb_rx.h
        mcu/rp2040.h mcu/rp2040.c
        printf-4.0.0/printf.c printf-4.0.0/printf.h
        font/font.h font/hunter-14pt-19h15w.h font/hunter-12pt-16h13w.h font/hunter-20pt-21h21w.h font/hunter-23pt-24h24w.h font/background.h font/background_image_v4.h
        fatfs/diskio.h fatfs/ff.c fatfs/ff.h fatfs/ffconf.h fatfs/ffsystem.c fatfs/ffunicode.c fatfs/tf_card.c fatfs/tf_card.h
        pio_i2c.c pio_i2c.h
        storage.h storage.c
        ui/ui_term.c ui/ui_term.h  ui/ui_statusbar.c ui/ui_statusbar.h
        system_monitor.h system_monitor.c
        mjson/mjson.h mjson/mjson.c 
        postprocess.c postprocess.h syntax.h syntax.c  syntax_struct.h
        dump.c dump.h opt_args.h command_attributes.h bytecode.h
        )
   
pico_generate_pio_header(bus_pirate ${CMAKE_CURRENT_LIST_DIR}/i2c.pio)
pico_generate_pio_header(bus_pirate ${CMAKE_CURRENT_LIST_DIR}/ws2812.pio)
pico_generate_pio_header(bus_pirate ${CMAKE_CURRENT_LIST_DIR}/apa102.pio)

pico_set_program_name(bus_pirate "bus_pirate")
pico_set_program_version(bus_pirate "0.0.4")
pico_set_program_description(bus_pirate "Bus Pirate Firmware for RP2040")

pico_enable_stdio_uart(bus_pirate 0)
pico_enable_stdio_usb(bus_pirate 0)

# Add the standard library to the build
target_link_libraries(bus_pirate pico_stdlib)

target_include_directories(bus_pirate PUBLIC ${CMAKE_CURRENT_LIST_DIR} . )

# Add any user requested libraries
target_link_libraries(bus_pirate
        hardware_spi
        hardware_i2c
        hardware_dma
        hardware_pio
        hardware_interp
        hardware_timer
        hardware_watchdog
        hardware_clocks
        hardware_adc
        hardware_uart
        pico_multicore
        pico_unique_id
        pico_bootrom 
        hardware_pwm
        tinyusb_device
        tinyusb_board
        )

pico_add_extra_outputs(bus_pirate)

