cmake_minimum_required(VERSION 3.25)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17) 

# initalize pico_sdk from installed location
# (note this can come from environment, CMake cache etc)
#set(PICO_SDK_PATH "C:/pico113/pico-sdk")

# Add my_pico_w.h
set(PICO_BOARD_HEADER_DIRS ${CMAKE_CURRENT_LIST_DIR}/boards ) 
# Add my_pico_w.cmake
set(PICO_BOARD_CMAKE_DIRS ${CMAKE_CURRENT_LIST_DIR}/boards )
# Add my_pico_w
#
#set(BP_PICO_PLATFORM "rp2350")

if(DEFINED GIT_COMMIT_HASH)
    set(BP_FIRMWARE_HASH ${GIT_COMMIT_HASH})
else()
    set(BP_FIRMWARE_HASH "unknown")
endif()
message(GIT_COMMIT_HASH="${GIT_COMMIT_HASH}")
message(BP_FIRMWARE_HASH="${BP_FIRMWARE_HASH}")

if(${BP_PICO_PLATFORM} MATCHES "rp2350")
    set(PICO_PLATFORM "rp2350") #compile for 5XL and 6 
    set(PICO_BOARD buspirate6)   
else()
    set(PICO_PLATFORM "rp2040") #compile for 5 rev8 and 5 rev10
    set(PICO_BOARD buspirate5)
endif()



# Pull in Raspberry Pi Pico SDK (must be before project)
include(cmake/pico_sdk_import.cmake)

project(bus_pirate C CXX ASM) 

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()  

# build time in UTC ISO 8601
FILE (WRITE ${CMAKE_BINARY_DIR}/timestamp.cmake "STRING(TIMESTAMP TIMEZ UTC)\n")
FILE (APPEND ${CMAKE_BINARY_DIR}/timestamp.cmake "FILE(WRITE timestamp.h \"#ifndef TIMESTAMP_H\\n\")\n")
FILE (APPEND ${CMAKE_BINARY_DIR}/timestamp.cmake "FILE(APPEND timestamp.h \"#define TIMESTAMP_H\\n\\n\")\n")
FILE (APPEND ${CMAKE_BINARY_DIR}/timestamp.cmake "FILE(APPEND timestamp.h \"#define _TIMEZ_ \\\"\${TIMEZ}\\\"\\n\\n\")\n")
FILE (APPEND ${CMAKE_BINARY_DIR}/timestamp.cmake "FILE(APPEND timestamp.h \"#endif // TIMESTAMP_H\\n\")\n")
ADD_CUSTOM_TARGET (
    timestamp
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_BINARY_DIR}/timestamp.cmake
    ADD_DEPENDENCIES ${CMAKE_BINARY_DIR}/timestamp.cmake)

# Add executable. Default name is the project name, version 0.1
set(bp5_common
        # main loop and printf
        pirate.c pirate.h printf-4.0.0/printf.c printf-4.0.0/printf.h
        modes.c modes.h system_config.h system_config.c
        system_monitor.h system_monitor.c

        # pirate lib
        pirate/psu.h pirate/psu.c pirate/pullup.h pirate/pullup.c
        pirate/mcu.h pirate/mcu.c pirate/lsb.c pirate/lsb.h
        pirate/bio.h pirate/bio.c pirate/storage.h pirate/storage.c
        pirate/button.h pirate/button.c pirate/mem.c pirate/mem.h
        pirate/lcd.h pirate/lcd.c
        pirate/amux.h pirate/amux.c pirate/rgb.h pirate/rgb.c
        pirate/intercore_helpers.c

        # commands
        commands.h commands.c
        commands/global/w_psu.h commands/global/w_psu.c
        commands/global/p_pullups.h commands/global/p_pullups.c
        commands/global/cmd_mcu.h commands/global/cmd_mcu.c
        commands/global/l_bitorder.h commands/global/l_bitorder.c
        commands/global/cmd_convert.h commands/global/cmd_convert.c
        commands/global/pause.h commands/global/pause.c
        commands/global/h_help.h commands/global/h_help.c
        commands/global/cmd_selftest.h commands/global/cmd_selftest.c
        commands/global/a_auxio.h commands/global/a_auxio.c
        commands/global/dummy.c commands/global/dummy.h
        commands/global/disk.c commands/global/disk.h
        commands/global/v_adc.c commands/global/v_adc.h
        commands/global/i_info.c commands/global/i_info.h
        commands/global/pwm.c commands/global/pwm.h
        commands/global/freq.c commands/global/freq.h
        commands/global/macro.c commands/global/macro.h
        commands/global/script.c commands/global/script.h
        commands/global/tutorial.c commands/global/tutorial.h
        commands/global/button_scr.c commands/global/button_scr.h
        commands/global/smps.c commands/global/smps.h
        commands/global/cls.h commands/global/cls.c  
        commands/global/cmd_binmode.h commands/global/cmd_binmode.c
        commands/global/logic.h commands/global/logic.c
        commands/global/bug.c commands/global/bug.h
        
        # HiZ
        mode/hiz.h mode/hiz.c

        # 1Wire
        mode/hw1wire.h mode/hw1wire.c
        pirate/hw1wire_pio.h pirate/hw1wire_pio.c
        commands/1wire/scan.h commands/1wire/scan.c
        commands/1wire/demos.h commands/1wire/demos.c
        pirate/onewire_library.h pirate/onewire_library.c

        # UART
        mode/hwuart.h mode/hwuart.c
        pirate/hwuart_pio.h pirate/hwuart_pio.c
        commands/uart/nmea.c commands/uart/nmea.h
        commands/uart/bridge.h commands/uart/bridge.c
        commands/uart/simcard.h commands/uart/simcard.c
        lib/minmea/minmea.h lib/minmea/minmea.c lib/minmea/gps.h lib/minmea/gps.c
        commands/uart/monitor.h commands/uart/monitor.c

        # Half duplex UART
        mode/hwhduart.h mode/hwhduart.c
        commands/hdplxuart/bridge.c commands/hdplxuart/bridge.h

        # I2C 
        mode/hwi2c.c mode/hwi2c.h
        pirate/hwi2c_pio.c pirate/hwi2c_pio.h
        commands/i2c/scan.h commands/i2c/scan.c
        commands/i2c/demos.h commands/i2c/demos.c
        lib/i2c_address_list/dev_i2c_addresses.h
        lib/ms5611/ms5611.c lib/ms5611/ms5611.h lib/tsl2561/driver_tsl2561.c lib/tsl2561/driver_tsl2561.h

        # SPI
        mode/hwspi.c mode/hwspi.h
        pirate/hwspi.c pirate/hwspi.h
        commands/spi/flash.c commands/spi/flash.h commands/spi/spiflash.h commands/spi/spiflash.c
        commands/spi/sniff.c commands/spi/sniff.h
        lib/sfud/inc/sfud.h lib/sfud/inc/sfud.c lib/sfud/inc/sfud_cfg.h lib/sfud/inc/sfud_def.h
        lib/sfud/inc/sfud_flash_def.h lib/sfud/inc/sfud_port.c lib/sfud/inc/sfud_sfdp.c

        # 2WIRE
        mode/hw2wire.h mode/hw2wire.c
        pirate/hw2wire_pio.h pirate/hw2wire_pio.c
        commands/2wire/sle4442.c commands/2wire/sle4442.h

        # LED
        mode/hwled.c mode/hwled.h

        # DIO
        mode/dio.h mode/dio.c

        #Infrared
        mode/infrared.h mode/infrared.c 
        lib/pico_ir_nec/nec_transmit.c lib/pico_ir_nec/nec_transmit.h

        # DUMMY MODE
        mode/dummy1.c mode/dummy1.h 

        # UI
        ui/ui_button.c ui/ui_button.h
        ui/ui_cmdln.c ui/ui_cmdln.h
        ui/ui_process.h ui/ui_process.c
        ui/ui_flags.h
        ui/ui_parse.c ui/ui_parse.h
        ui/ui_prompt.h ui/ui_prompt.c
        ui/ui_mode.c ui/ui_mode.h
        ui/ui_display.c ui/ui_display.h
        ui/ui_info.h ui/ui_info.c
        ui/ui_format.c ui/ui_format.c
        ui/ui_init.c ui/ui_init.h
        ui/ui_const.h ui/ui_const.c
        ui/ui_config.h ui/ui_config.c
        ui/ui_help.c ui/ui_help.h
        ui/ui_term.c ui/ui_term.h
        ui/ui_statusbar.c ui/ui_statusbar.h
        debug.c debug.h

        # JSON parser for saved settings
        mjson/mjson.h mjson/mjson.c

        # syntax and commands
        syntax.h syntax.c syntax_struct.h
        opt_args.h command_attributes.h bytecode.h
        pio_config.h

        # FATfs
        fatfs/diskio.c fatfs/diskio.h fatfs/ff.c fatfs/ff.h fatfs/ffconf.h fatfs/ffsystem.c fatfs/ffunicode.c

        # translations
        translation/base.h translation/base.c
        translation/en-us.h translation/zh-cn.h translation/pl-pl.h

        # binary mode
        binmode/binmodes.c binmode/binmodes.h binmode/binio.c binmode/binio.h  
        binmode/binio_helpers.c binmode/binio_helpers.h
        mode/binloopback.c mode/binloopback.h
        binmode/logicanalyzer.h binmode/logicanalyzer.c binmode/sump.c binmode/sump.h
        binmode/dirtyproto.c binmode/dirtyproto.h
        binmode/legacy4third.c binmode/legacy4third.h
        lib/arduino-ch32v003-swio/arduino_ch32v003.c lib/arduino-ch32v003-swio/arduino_ch32v003.h
        lib/arduino-ch32v003-swio/swio.h lib/arduino-ch32v003-swio/swio.c     
        lib/picorvd/picoswio.c lib/picorvd/picoswio.h lib/picorvd/debug_defines.h
        binmode/fala.c binmode/fala.h binmode/falaio.c binmode/falaio.h

        #toolbars
        toolbars/logic_bar.c toolbars/logic_bar.h

        # displays
        displays.c displays.h display/default.c display/default.h
        ui/ui_lcd.c ui/ui_lcd.h display/scope.h display/scope.c
        font/font.h font/hunter-14pt-19h15w.h font/hunter-12pt-16h13w.h font/hunter-20pt-21h21w.h
        font/hunter-23pt-24h24w.h font/background.h font/background_image_v4.h

        # USB
        queue.c queue.h
        usb_tx.c usb_tx.h usb_rx.c usb_rx.h
        msc_disk.c msc_disk.h usb_descriptors.c
)
set(bp5_nandflash
        #nand flash management and dhara
        dhara/bytes.h dhara/error.c dhara/error.h dhara/journal.c dhara/journal.h dhara/map.c dhara/map.h dhara/nand.c dhara/nand.h
        nand/nand_ftl_diskio.c nand/nand_ftl_diskio.h nand/spi.c nand/spi.h nand/spi_nand.c nand/spi_nand.h 
        nand/sys_time.c nand/sys_time.h
)

if(${PICO_PLATFORM} MATCHES "rp2350")
        add_executable(bus_pirate5_xl
                ${bp5_common}
                ${bp5_nandflash}
                platform/bpi-rev10.h platform/bpi-rev10.c 
                pirate/shift.h pirate/shift.c
                )
        target_compile_definitions(bus_pirate5_xl PUBLIC BP_VER=XL5)
        target_compile_definitions(bus_pirate5_xl PUBLIC BP_REV=10)
        target_compile_definitions(bus_pirate5_xl PUBLIC BP_FIRMWARE_HASH="${BP_FIRMWARE_HASH}")

        add_executable(bus_pirate6
                ${bp5_common}
                ${bp5_nandflash}
                platform/bpi6-rev1.h platform/bpi6-rev1.c 
                )
        target_compile_definitions(bus_pirate6 PUBLIC BP_VER=6) 
        target_compile_definitions(bus_pirate6 PUBLIC BP_REV=10) 
        target_compile_definitions(bus_pirate6 PUBLIC BP_FIRMWARE_HASH="${BP_FIRMWARE_HASH}") 
        
        set(revisions 
        bus_pirate5_xl  
        bus_pirate6
        )        

else()
        add_executable(bus_pirate5_rev8
                ${bp5_common}
                platform/bpi-rev8.h platform/bpi-rev8.c 
                #tf card access control
                fatfs/tf_card.c fatfs/tf_card.h
                pirate/shift.h pirate/shift.c
                )
        target_compile_definitions(bus_pirate5_rev8 PUBLIC BP_VER=5) 
        target_compile_definitions(bus_pirate5_rev8 PUBLIC BP_REV=8)
        target_compile_definitions(bus_pirate5_rev8 PUBLIC BP_FIRMWARE_HASH="${BP_FIRMWARE_HASH}")

        add_executable(bus_pirate5_rev10
                ${bp5_common}
                ${bp5_nandflash}
                platform/bpi-rev10.h platform/bpi-rev10.c 
                pirate/shift.h pirate/shift.c
                )
        target_compile_definitions(bus_pirate5_rev10 PUBLIC BP_VER=5)                
        target_compile_definitions(bus_pirate5_rev10 PUBLIC BP_REV=10)
        target_compile_definitions(bus_pirate5_rev10 PUBLIC BP_FIRMWARE_HASH="${BP_FIRMWARE_HASH}")

        set(revisions
        bus_pirate5_rev8  
        bus_pirate5_rev10
        )
endif()


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



# Compile the various PIO programs to generate the header files
foreach(revision ${revisions})
        ADD_DEPENDENCIES (${revision} timestamp)
        pico_generate_pio_header(${revision} ${CMAKE_CURRENT_LIST_DIR}/pirate/hw2wire.pio)
        pico_generate_pio_header(${revision} ${CMAKE_CURRENT_LIST_DIR}/pirate/hwi2c.pio)
        pico_generate_pio_header(${revision} ${CMAKE_CURRENT_LIST_DIR}/pirate/ws2812.pio)
        pico_generate_pio_header(${revision} ${CMAKE_CURRENT_LIST_DIR}/pirate/apa102.pio) 
        pico_generate_pio_header(${revision} ${CMAKE_CURRENT_LIST_DIR}/pirate/hw1wire.pio) 
        pico_generate_pio_header(${revision} ${CMAKE_CURRENT_LIST_DIR}/pirate/hwuart.pio)  
        pico_generate_pio_header(${revision} ${CMAKE_CURRENT_LIST_DIR}/pirate/spisnif.pio)  
        pico_generate_pio_header(${revision} ${CMAKE_CURRENT_LIST_DIR}/binmode/logicanalyzer.pio)
        pico_generate_pio_header(${revision} ${CMAKE_CURRENT_LIST_DIR}/lib/pico_ir_nec/nec_carrier_burst.pio)
        pico_generate_pio_header(${revision} ${CMAKE_CURRENT_LIST_DIR}/lib/pico_ir_nec/nec_carrier_control.pio)
        pico_generate_pio_header(${revision} ${CMAKE_CURRENT_LIST_DIR}/lib/picorvd/ch32vswio.pio)
        #pico_generate_pio_header(${revision} ${CMAKE_CURRENT_LIST_DIR}/pirate/pwm.pio)
        pico_generate_pio_header(${revision} ${CMAKE_CURRENT_LIST_DIR}/pirate/onewire_library.pio)
        pico_generate_pio_header(${revision} ${CMAKE_CURRENT_LIST_DIR}/lib/pio_pwm/pwm.pio)

        pico_set_program_name(${revision} ${revision}) 
        pico_set_program_version(${revision} "0.1.0")
        if(${PICO_PLATFORM} MATCHES "rp2350")
                pico_set_program_description(${revision} "Bus Pirate 5XL/6 Firmware for RP2350")
        else()
                pico_set_program_description(${revision} "Bus Pirate 5 Firmware for RP2040")
        endif()
        pico_enable_stdio_uart(${revision} 0)
        pico_enable_stdio_usb(${revision} 0)

        #XOSC fix?
        target_compile_definitions(${revision} PUBLIC PICO_XOSC_STARTUP_DELAY_MULTIPLIER=172)
        # useful for debug builds
        target_compile_definitions(${revision} PUBLIC PICO_USE_STACK_GUARDS=$<CONFIG:DEBUG>)
        target_compile_definitions(${revision} PUBLIC PICO_STACK_SIZE=4096)        

        # Add the standard library to the build
        target_link_libraries(${revision} ${stdlibs} )
        target_include_directories(${revision} PUBLIC ${CMAKE_CURRENT_LIST_DIR} . )
        pico_add_extra_outputs(${revision})
        if(${PICO_PLATFORM} MATCHES "rp2350")
                pico_set_linker_script(${revision} ${CMAKE_CURRENT_LIST_DIR}/boards/memmap_default_rp2350.ld)
        else()
                pico_set_linker_script(${revision} ${CMAKE_CURRENT_LIST_DIR}/boards/memmap_default_rp2040.ld)
        endif()
        #pico_set_linker_script(${revision} ${CMAKE_CURRENT_LIST_DIR}/pirate/memmap_default.ld)
endforeach()

string(APPEND CMAKE_EXE_LINKER_FLAGS "-Wl,--print-memory-usage")


# by default this project uses LGPL3 libraries, if you do not want to comply
# with LGPL3 license, please set the following variable to FALSE, otherwise
# you are oblicated to distribute the resulted binaries under LGPL3 license
set(USE_LGPL3 TRUE CACHE BOOL "indicated if to include LGPL3 protected code")
include(cmake/ansi_colours_import.cmake)
if(USE_LGPL3)
if(LEGACY_ANSI_COLOURS_ENABLED)
        if(${PICO_PLATFORM} MATCHES "rp2350")
                target_link_libraries(bus_pirate5_xl         lib_ansi_colours)
                target_link_libraries(bus_pirate6           lib_ansi_colours)
        else()
                target_link_libraries(bus_pirate5_rev10       lib_ansi_colours)
                target_link_libraries(bus_pirate5_rev8        lib_ansi_colours)
        endif()
endif()
endif()

 
