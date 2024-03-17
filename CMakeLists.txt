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

#add_compile_definitions(BP_FIRMWARE_HASH=${BP_FIRMWARE_HASH})
#add_compile_definitions(BP5_REV=8) 

# Add executable. Default name is the project name, version 0.1
set(bp5_common 
        pirate.c pirate.h commands.h commands.c 
        mode/sump.c mode/sump.h        
        ui/ui_lcd.c ui/ui_lcd.h rgb.c rgb.h bio.h bio.c
	msc_disk.c usb_descriptors.c
	displays.c displays.h display/default.c display/default.h
        shift.h shift.c psu.h psu.c amux.h amux.c button.c button.h
        system_config.h system_config.c modes.c modes.h auxpinfunc.h auxpinfunc.c
        pwm.c pwm.h freq.c freq.h adc.c adc.h pullups.c pullups.h helpers.h helpers.c
        mode/dummy1.c mode/dummy1.h mode/hiz.h mode/hiz.c 
        mode/hwspi.c mode/hwspi.h mode/spiflash.h mode/spiflash.c
        lib/sfud/inc/sfud.h lib/sfud/inc/sfud.c lib/sfud/inc/sfud_cfg.h lib/sfud/inc/sfud_def.h lib/sfud/inc/sfud_flash_def.h lib/sfud/inc/sfud_port.c lib/sfud/inc/sfud_sfdp.c        mode/usart.h mode/usart.c 
        mode/hwi2c.c mode/hwi2c.h lib/i2c_address_list/dev_i2c_addresses.h mode/pio_i2c.c mode/pio_i2c.h
        mode/hwled.c mode/hwled.h
        ui/ui_cmdln.c ui/ui_cmdln.h ui/ui_process.h ui/ui_process.c
        mode/hw1wire.h mode/hw1wire.c mode/onewire.h mode/onewire.c
	display/scope.h display/scope.c
        ui/ui_flags.h
        debug.c debug.h  
        ui/ui_parse.c ui/ui_parse.h ui/ui_prompt.h ui/ui_prompt.c  ui/ui_mode.c ui/ui_mode.h
	ui/ui_display.c ui/ui_display.h
        ui/ui_info.h ui/ui_info.c ui/ui_format.c ui/ui_format.c ui/ui_init.c ui/ui_init.h
        ui/ui_const.h ui/ui_const.c ui/ui_config.h ui/ui_config.c 
        ui/ui_help.c ui/ui_help.h
        translation/base.h translation/base.c translation/en-us.h translation/zh-cn.h translation/pl-pl.h
        queue.c queue.h 
        usb_tx.c usb_tx.h usb_rx.c usb_rx.h
        mcu/rp2040.h mcu/rp2040.c
        printf-4.0.0/printf.c printf-4.0.0/printf.h
        font/font.h font/hunter-14pt-19h15w.h font/hunter-12pt-16h13w.h font/hunter-20pt-21h21w.h font/hunter-23pt-24h24w.h font/background.h font/background_image_v4.h
        fatfs/diskio.c fatfs/diskio.h fatfs/ff.c fatfs/ff.h fatfs/ffconf.h fatfs/ffsystem.c fatfs/ffunicode.c 

        storage.h storage.c
        ui/ui_term.c ui/ui_term.h  ui/ui_statusbar.c ui/ui_statusbar.h
        system_monitor.h system_monitor.c
        mjson/mjson.h mjson/mjson.c 
        postprocess.c postprocess.h syntax.h syntax.c  syntax_struct.h
        flash.c flash.h 
        opt_args.h ui/ui_args.c ui/ui_args.h
        command_attributes.h bytecode.h
        lib/minmea/minmea.h lib/minmea/minmea.c lib/minmea/gps.h lib/minmea/gps.c lib/ms5611/ms5611.c lib/ms5611/ms5611.h
        lib/tsl2561/driver_tsl2561.c  lib/tsl2561/driver_tsl2561.h
        lib/sle4442/sle4442.c lib/sle4442/sle4442.h
        mode/binio.c mode/binio.h mode/binio_helpers.c mode/binio_helpers.h
        #mem buffer for shared malloc
        mem.c mem.h
        mode/logicanalyzer.h mode/logicanalyzer.c
        mode/hw2wire.h mode/hw2wire.c mode/hw2wire_pio.h mode/hw2wire_pio.c
        dummy.c dummy.h
        )

add_executable(bus_pirate5_rev8
        ${bp5_common}
        platform/bpi-rev8.h platform/bpi-rev8.c 
        #tf card access control
        fatfs/tf_card.c fatfs/tf_card.h
        )

target_compile_definitions(bus_pirate5_rev8 PUBLIC BP5_REV=8)

add_executable(bus_pirate5_rev10
        ${bp5_common}
        platform/bpi-rev10.h platform/bpi-rev10.c 
        #nand flash management and dhara
        dhara/bytes.h dhara/error.c dhara/error.h dhara/journal.c dhara/journal.h dhara/map.c dhara/map.h dhara/nand.c dhara/nand.h
        nand/nand_ftl_diskio.c nand/nand_ftl_diskio.h nand/spi.c nand/spi.h nand/spi_nand.c nand/spi_nand.h 
        nand/sys_time.c nand/sys_time.h
        )
target_compile_definitions(bus_pirate5_rev10 PUBLIC BP5_REV=10)

set(stdlibs
        pico_stdlib
        hardware_spi
        hardware_i2c
        hardware_dma
        hardware_pio
        hardware_interp
        hardware_timer
        hardware_watchdog
        hardware_clocks
        hardware_flash
        hardware_adc
        hardware_uart
        pico_multicore
        pico_base_headers
        pico_unique_id
        pico_bootrom 
        hardware_pwm
        tinyusb_device
        tinyusb_board
        hardware_structs
        )        

set(revisions
        bus_pirate5_rev8  
        bus_pirate5_rev10
        )

foreach(revision ${revisions})
        pico_generate_pio_header(${revision} ${CMAKE_CURRENT_LIST_DIR}/mode/hw2wire.pio)
        pico_generate_pio_header(${revision} ${CMAKE_CURRENT_LIST_DIR}/mode/i2c.pio)
        pico_generate_pio_header(${revision} ${CMAKE_CURRENT_LIST_DIR}/ws2812.pio)
        pico_generate_pio_header(${revision} ${CMAKE_CURRENT_LIST_DIR}/apa102.pio)
        pico_generate_pio_header(${revision} ${CMAKE_CURRENT_LIST_DIR}/mode/onewire.pio)
        pico_generate_pio_header(${revision} ${CMAKE_CURRENT_LIST_DIR}/mode/logicanalyzer.pio)

        pico_set_program_name(${revision} ${revision})
        pico_set_program_version(${revision} "0.1.0")
        pico_set_program_description(${revision} "Bus Pirate 5 Firmware for RP2040")

        pico_enable_stdio_uart(${revision} 0)
        pico_enable_stdio_usb(${revision} 0)

        # Add the standard library to the build
        target_link_libraries(${revision} ${stdlibs} )
        target_include_directories(${revision} PUBLIC ${CMAKE_CURRENT_LIST_DIR} . )
        pico_add_extra_outputs(${revision})
endforeach()

string(APPEND CMAKE_EXE_LINKER_FLAGS "-Wl,--print-memory-usage")

 
