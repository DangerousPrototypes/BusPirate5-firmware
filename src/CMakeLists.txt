
# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()  

#  set all the warnings to help reduce bugs
# set(BP_PENDANTIC 1)


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
set(buspirate_common
        # main loop and printf
        pirate.c
        pirate.h
        printf-4.0.0/printf.c
        printf-4.0.0/printf.h
        modes.c
        modes.h
        system_config.h
        system_config.c
        system_monitor.h
        system_monitor.c

        #flatcc test
        commands/global/flat.h 
        commands/global/flat.c 
        bpio_builder.h
        bpio_reader.h
        bpio_verifier.h
        monster_verifier.h 
        flatbuffers_common_builder.h 
        flatbuffers_common_reader.h
        monster_builder.h
        monster_reader.h
        builder.c
        emitter.c 
        json_parser.c 
        json_printer.c 
        refmap.c 
        verifier.c 
        flatcc/flatcc_portable.h
        flatcc/flatcc_rtconfig.h
        flatcc/flatcc_unaligned.h
        flatcc/flatcc_flatbuffers.h
        flatcc/flatcc_verifier.h
        flatcc/flatcc_prologue.h
        flatcc/flatcc.h
        flatcc/flatcc_json_printer.h
        flatcc/flatcc_assert.h
        flatcc/flatcc_json_parser.h
        flatcc/flatcc_version.h
        flatcc/flatcc_endian.h
        flatcc/flatcc_emitter.h
        flatcc/flatcc_accessors.h
        flatcc/flatcc_refmap.h
        flatcc/flatcc_types.h
        flatcc/flatcc_builder.h
        flatcc/flatcc_alloc.h
        flatcc/flatcc_identifier.h
        flatcc/flatcc_epilogue.h
        flatcc/flatcc_iov.h
        flatcc/support/elapsed.h
        flatcc/support/cdump.h
        flatcc/support/hexdump.h
        flatcc/support/readfile.h
        flatcc/reflection/reflection_verifier.h
        flatcc/reflection/reflection_reader.h
        flatcc/reflection/reflection_builder.h
        flatcc/reflection/flatbuffers_common_builder.h
        flatcc/reflection/flatbuffers_common_reader.h
        flatcc/portable/portable.h
        flatcc/portable/pinttypes.h
        flatcc/portable/pdiagnostic_push.h
        flatcc/portable/pinline.h
        flatcc/portable/pendian_detect.h
        flatcc/portable/pversion.h
        flatcc/portable/pdiagnostic_pop.h
        flatcc/portable/pstdalign.h
        flatcc/portable/pstatic_assert_scope.h
        flatcc/portable/punaligned.h
        flatcc/portable/grisu3_math.h
        flatcc/portable/paligned_alloc.h
        flatcc/portable/pprintfp.h
        flatcc/portable/pattributes.h
        flatcc/portable/pmemaccess.h
        flatcc/portable/pstdint.h
        flatcc/portable/pstdbool.h
        flatcc/portable/pwarnings.h
        flatcc/portable/portable_basic.h
        flatcc/portable/pprintint.h
        flatcc/portable/pparseint.h
        flatcc/portable/grisu3_parse.h
        flatcc/portable/grisu3_print.h
        flatcc/portable/prestrict.h
        flatcc/portable/pbase64.h
        flatcc/portable/pparsefp.h
        flatcc/portable/pdiagnostic.h
        flatcc/portable/pendian.h
        flatcc/portable/pcrt.h
        flatcc/portable/pstatic_assert.h
        flatcc/portable/include/linux/endian.h
        flatcc/portable/include/std/inttypes.h
        flatcc/portable/include/std/stdbool.h
        flatcc/portable/include/std/stdint.h
        flatcc/portable/include/std/stdalign.h
        lib/nanocobs/cobs.c
        lib/nanocobs/cobs.h


        # pirate lib
        pirate/psu.h
        pirate/psu.c
        pirate/pullup.h
        pirate/pullup.c
        pirate/mcu.h
        pirate/mcu.c
        pirate/lsb.c
        pirate/lsb.h
        pirate/bio.h
        pirate/bio.c
        pirate/storage.h
        pirate/storage.c
        pirate/button.h
        pirate/button.c
        pirate/mem.c
        pirate/mem.h
        pirate/lcd.h
        pirate/lcd.c 
        pirate/amux.h
        pirate/amux.c
        pirate/rgb.h
        pirate/rgb.c
        pirate/intercore_helpers.c
        pirate/file.h 
        pirate/file.c

        # libraries
        lib/jep106/jep106.c
        lib/jep106/jep106.h

        # commands
        commands.h
        commands.c
        commands/global/w_psu.h
        commands/global/w_psu.c
        commands/global/p_pullups.h
        commands/global/p_pullups.c
        commands/global/cmd_mcu.h
        commands/global/cmd_mcu.c
        commands/global/l_bitorder.h
        commands/global/l_bitorder.c
        commands/global/cmd_convert.h
        commands/global/cmd_convert.c
        commands/global/pause.h
        commands/global/pause.c
        commands/global/h_help.h
        commands/global/h_help.c
        commands/global/cmd_selftest.h
        commands/global/cmd_selftest.c
        commands/global/a_auxio.h
        commands/global/a_auxio.c
        commands/global/dummy.c
        commands/global/dummy.h
        commands/global/disk.c
        commands/global/disk.h
        commands/global/v_adc.c
        commands/global/v_adc.h
        commands/global/i_info.c
        commands/global/i_info.h
        commands/global/pwm.c
        commands/global/pwm.h
        commands/global/freq.c
        commands/global/freq.h
        commands/global/macro.c
        commands/global/macro.h
        commands/global/script.c
        commands/global/script.h
        commands/global/button_scr.c
        commands/global/button_scr.h
        commands/global/smps.c
        commands/global/smps.h
        commands/global/cls.h
        commands/global/cls.c  
        commands/global/cmd_binmode.h
        commands/global/cmd_binmode.c
        commands/global/logic.h
        commands/global/logic.c
        commands/global/bug.c
        commands/global/bug.h
        commands/global/image.c
        commands/global/image.h
        commands/global/dump.c
        commands/global/dump.h
        commands/global/ovrclk.c 
        commands/global/ovrclk.h   
        commands/global/hex.c     
        commands/global/hex.h
        commands/eeprom/eeprom_base.h
        commands/eeprom/eeprom_base.c
        
        # HiZ
        mode/hiz.h
        mode/hiz.c

        # 1Wire
        mode/hw1wire.h
        mode/hw1wire.c
        pirate/hw1wire_pio.h
        pirate/hw1wire_pio.c
        commands/1wire/scan.h
        commands/1wire/scan.c
        commands/1wire/demos.h
        commands/1wire/demos.c
        pirate/onewire_library.h
        pirate/onewire_library.c
        commands/eeprom/eeprom_1wire.h
        commands/eeprom/eeprom_1wire.c
        commands/global/jep106_lookup.c
        commands/global/jep106_lookup.h

        # UART
        mode/hwuart.h
        mode/hwuart.c
        pirate/hwuart_pio.h
        pirate/hwuart_pio.c
        commands/uart/nmea.c
        commands/uart/nmea.h
        commands/uart/bridge.h
        commands/uart/bridge.c
        commands/uart/simcard.h
        commands/uart/simcard.c
        lib/minmea/minmea.h
        lib/minmea/minmea.c
        lib/minmea/gps.h
        lib/minmea/gps.c
        commands/uart/monitor.h
        commands/uart/monitor.c
        commands/uart/glitch.h
        commands/uart/glitch.c

        # Half duplex UART
        mode/hwhduart.h
        mode/hwhduart.c
        commands/hdplxuart/bridge.c
        commands/hdplxuart/bridge.h

        # I2C 
        mode/hwi2c.c
        mode/hwi2c.h
        pirate/hwi2c_pio.c
        pirate/hwi2c_pio.h
        commands/i2c/scan.h
        commands/i2c/scan.c
        commands/i2c/demos.h
        commands/i2c/demos.c
        lib/i2c_address_list/dev_i2c_addresses.h
        lib/ms5611/ms5611.c
        lib/ms5611/ms5611.h
        lib/tsl2561/driver_tsl2561.c
        lib/tsl2561/driver_tsl2561.h
        commands/i2c/sniff.c 
        commands/i2c/sniff.h
        commands/i2c/ddr5.c
        commands/i2c/ddr5.h
        commands/i2c/ddr4.c
        commands/i2c/ddr4.h        
        commands/eeprom/eeprom_i2c.c 
        commands/eeprom/eeprom_i2c.h
        commands/i2c/i2c.c
        commands/i2c/i2c.h
        commands/i2c/usbpdo.c 
        commands/i2c/usbpdo.h
        commands/i2c/usbpd.c 
        commands/i2c/usbpd.h
        lib/ap33772s/ap33772s.c
        lib/ap33772s/ap33772s.h 

        # SPI
        mode/hwspi.c
        mode/hwspi.h
        pirate/hwspi.c
        pirate/hwspi.h
        commands/spi/flash.c
        commands/spi/flash.h
        commands/spi/spiflash.h
        commands/spi/spiflash.c
        commands/spi/sniff.c
        commands/spi/sniff.h
#        commands/spi/universalprogrammer.h
#        commands/spi/universalprogrammer.c
        lib/sfud/inc/sfud.h
        lib/sfud/inc/sfud.c
        lib/sfud/inc/sfud_cfg.h
        lib/sfud/inc/sfud_def.h
        lib/sfud/inc/sfud_flash_def.h
        lib/sfud/inc/sfud_port.c
        lib/sfud/inc/sfud_sfdp.c
        commands/eeprom/eeprom_spi.h
        commands/eeprom/eeprom_spi.c

        # 2WIRE
        mode/hw2wire.h
        mode/hw2wire.c
        pirate/hw2wire_pio.h
        pirate/hw2wire_pio.c
        commands/2wire/sle4442.c
        commands/2wire/sle4442.h
        commands/2wire/hw2w_sniff.c 
        commands/2wire/hw2w_sniff.h

        # 3WIRE
        mode/hw3wire.h
        mode/hw3wire.c
        pirate/hw3wire_pio.h
        pirate/hw3wire_pio.c


        # LED
        mode/hwled.c
        mode/hwled.h

        # DIO
        mode/dio.h
        mode/dio.c

        #Infrared
        mode/infrared.h
        mode/infrared.c 
        lib/pico_ir_nec/nec_transmit.c
        lib/pico_ir_nec/nec_transmit.h
        lib/pico_ir_nec/nec_receive.c
        lib/pico_ir_nec/nec_receive.h
        pirate/rc5_pio.c
        pirate/rc5_pio.h
        mode/infrared-struct.h
        commands/infrared/tvbgone.c
        commands/infrared/tvbgone.h
        commands/infrared/tvbgone-codes.h
        lib/picofreq/picofreq.c
        lib/picofreq/picofreq.h
        commands/infrared/irtxrx.c
        commands/infrared/irtxrx.h

        # JTAG
        mode/jtag.h
        mode/jtag.c
        commands/jtag/bluetag.c
        commands/jtag/bluetag.h
        lib/bluetag/src/blueTag.c
        lib/bluetag/src/blueTag.h
        lib/bluetag/src/jep106.inc

        # DUMMY MODE
        mode/dummy1.c
        mode/dummy1.h 
        
        # UNIVERSAL PROGRAMMER MODE
        mode/up.c
        mode/up.h
        commands/up/test.c
        commands/up/test.h
	commands/up/logicic.c
	commands/up/logicic.h
	commands/up/pic.c
	commands/up/pic.h
	commands/up/ram.c
	commands/up/ram.h
	commands/up/display.c
	commands/up/display.h
	commands/up/spirom.c
	commands/up/spirom.h
	commands/up/eprom.c
	commands/up/eprom.h
	commands/up/buffer.c
	commands/up/buffer.h
	lib/crc/crc_crc16.c
	lib/crc/crc_crc32.c
	lib/crc/crc_zip.c
	lib/crc/crc.h
	
        # UI
        ui/ui_cmdln.c
        ui/ui_cmdln.h
        ui/ui_process.h
        ui/ui_process.c
        ui/ui_flags.h
        ui/ui_parse.c
        ui/ui_parse.h
        ui/ui_prompt.h
        ui/ui_prompt.c
        ui/ui_mode.c
        ui/ui_mode.h
        ui/ui_display.c
        ui/ui_display.h
        ui/ui_info.h
        ui/ui_info.c
        ui/ui_format.c
        ui/ui_format.c
        ui/ui_init.c
        ui/ui_init.h
        ui/ui_const.h
        ui/ui_const.c
        ui/ui_config.h
        ui/ui_config.c
        ui/ui_help.c
        ui/ui_help.h
        ui/ui_term.c
        ui/ui_term.h
        ui/ui_statusbar.c
        ui/ui_statusbar.h
        ui/ui_hex.c
        ui/ui_hex.h
        ui/ui_progress_indicator.h
        ui/ui_progress_indicator.c


        # Debug
        debug_uart.c
        debug_uart.h
        debug_rtt.c
        debug_rtt.h

        # JSON parser for saved settings
        mjson/mjson.h
        mjson/mjson.c

        # syntax and commands
        syntax.h
        syntax.c
        syntax_struct.h
        command_struct.h
        bytecode.h
        pio_config.h

        # FATfs
        fatfs/diskio.c
        fatfs/diskio.h
        fatfs/ff.c
        fatfs/ff.h
        fatfs/ffconf.h
        fatfs/ffsystem.c
        fatfs/ffunicode.c

        # translations
        translation/base.h
        translation/base.c
        translation/en-us.h
        translation/zh-cn.h
        translation/pl-pl.h

        # binary mode
        binmode/binmodes.c
        binmode/binmodes.h
        binmode/binio_helpers.c
        binmode/binio_helpers.h
        mode/binloopback.c
        mode/binloopback.h
        binmode/logicanalyzer.h
        binmode/logicanalyzer.c
        binmode/sump.c
        binmode/sump.h
        binmode/bpio.c
        binmode/bpio.h
        binmode/bpio_transactions.h
        binmode/bpio_1wire.c
        binmode/bpio_1wire.h
        binmode/bpio_i2c.c
        binmode/bpio_i2c.h
        binmode/bpio_spi.c
        binmode/bpio_spi.h
        binmode/bpio_2wire.c
        binmode/bpio_2wire.h
        binmode/bpio_3wire.c
        binmode/bpio_3wire.h
        binmode/bpio_uart.c
        binmode/bpio_uart.h
        binmode/bpio_hiz.c
        binmode/bpio_hiz.h
        binmode/bpio_dio.c
        binmode/bpio_dio.h
        binmode/bpio_led.c
        binmode/bpio_led.h
        binmode/bpio_infrared.c
        binmode/bpio_infrared.h
        binmode/legacy4third.c
        binmode/legacy4third.h
        lib/arduino-ch32v003-swio/arduino_ch32v003.c
        lib/arduino-ch32v003-swio/arduino_ch32v003.h
        lib/arduino-ch32v003-swio/swio.h
        lib/arduino-ch32v003-swio/swio.c
        lib/picorvd/picoswio.c
        lib/picorvd/picoswio.h
        lib/picorvd/debug_defines.h
        binmode/fala.c
        binmode/fala.h
        binmode/falaio.c
        binmode/falaio.h
        binmode/irtoy-air.h
        binmode/irtoy-air.c
        pirate/irio_pio.h
        pirate/irio_pio.c
        binmode/irtoy-irman.h
        binmode/irtoy-irman.c

        #toolbars
        toolbars/logic_bar.c
        toolbars/logic_bar.h

        # displays
        displays.c
        displays.h
        display/default.c
        display/default.h
        display/disabled.c
        display/disabled.h
        ui/ui_lcd.c
        ui/ui_lcd.h
        display/scope.h
        display/scope.c
        font/font.h
        font/hunter-14pt-19h15w.h
        font/hunter-12pt-16h13w.h
        font/hunter-20pt-21h21w.h
        font/hunter-23pt-24h24w.h
        display/background.h
        display/background_image_v4.h
        display/robot16.h
        
        # USB - SPSC queues for inter-core communication
        spsc_queue.h
        usb_tx.c
        usb_tx.h
        usb_rx.c
        usb_rx.h
        msc_disk.c
        msc_disk.h
        usb_descriptors.c

	# USBPD 
        mode/usbpd.c            
        mode/usbpd.h

        # I2S
        mode/i2s.c
        mode/i2s.h
        commands/i2s/sine.h 
        commands/i2s/sine.c
)
set(buspirate_nandflash
        #nand flash management and dhara
        dhara/bytes.h
        dhara/error.c
        dhara/error.h
        dhara/journal.c
        dhara/journal.h
        dhara/map.c
        dhara/map.h
        dhara/nand.c
        dhara/nand.h
        nand/nand_ftl_diskio.c
        nand/nand_ftl_diskio.h
        nand/spi.c
        nand/spi.h
        nand/spi_nand.c
        nand/spi_nand.h
        nand/sys_time.c
        nand/sys_time.h
)

set (buspirate_rtt
        lib/rtt/Config/SEGGER_RTT_Conf.h
        lib/rtt/RTT/SEGGER_RTT.c
        lib/rtt/RTT/SEGGER_RTT.h
        lib/rtt/RTT/SEGGER_RTT_printf.c
)

set (rp2350_common
        commands/global/otpdump.h 
        commands/global/otpdump.c
)


if(${PICO_PLATFORM} MATCHES "rp2350")
        set(revisions
                bus_pirate5_xl  
                bus_pirate6
#                bus_pirate7
        )   

#        add_executable(bus_pirate7
#                ${buspirate_common}
#                ${buspirate_nandflash}
#                ${buspirate_rtt}
#                ${rp2350_common}
#                platform/bpi7-rev0.h
#                platform/bpi7-rev0.c
#                display/robot6x16.h
#                ) 
#        target_compile_definitions(bus_pirate7 PUBLIC BP_VER=7) 
#        target_compile_definitions(bus_pirate7 PUBLIC BP_REV=0) 
#        target_compile_definitions(bus_pirate7 PUBLIC BP_FIRMWARE_HASH="${BP_FIRMWARE_HASH}")             
#if(DEFINED BP_PENDANTIC)
#        target_compile_options(    bus_pirate7 PRIVATE -Wpedantic -Wall -Werror -O3 -Wno-unknown-pragmas -Wno-inline -Wno-unused-function)
#endif()

        add_executable(bus_pirate6
                ${buspirate_common}
                ${buspirate_nandflash}
                ${buspirate_rtt}
                ${rp2350_common}
                platform/bpi6-rev2.h
                platform/bpi6-rev2.c
                display/robot6x16.h
                )
        target_compile_definitions(bus_pirate6 PUBLIC BP_VER=6) 
        target_compile_definitions(bus_pirate6 PUBLIC BP_REV=10) 
        target_compile_definitions(bus_pirate6 PUBLIC BP_FIRMWARE_HASH="${BP_FIRMWARE_HASH}") 
        if(DEFINED BP_PENDANTIC)
        target_compile_options(    bus_pirate6 PRIVATE -Wpedantic -Wall -Werror -O3 -Wno-unknown-pragmas -Wno-inline -Wno-unused-function)
        endif()

        add_executable(bus_pirate5_xl
                ${buspirate_common}
                ${buspirate_nandflash}
                ${buspirate_rtt}
                ${rp2350_common}
                platform/bpi5xl-rev0.h
                platform/bpi5xl-rev0.c 
                pirate/shift.h
                pirate/shift.c
                display/robot5xx16.h
                )
        target_compile_definitions(bus_pirate5_xl PUBLIC BP_VER=XL5)
        target_compile_definitions(bus_pirate5_xl PUBLIC BP_REV=10)
        target_compile_definitions(bus_pirate5_xl PUBLIC BP_FIRMWARE_HASH="${BP_FIRMWARE_HASH}")
        if(DEFINED BP_PENDANTIC)
        target_compile_options(    bus_pirate5_xl PRIVATE -Wpedantic -Wall -Werror -O3 -Wno-unknown-pragmas -Wno-inline -Wno-unused-function)
        endif()
else()

        set(revisions
                bus_pirate5_rev8  
                bus_pirate5_rev10
        )

        add_executable(bus_pirate5_rev10
                ${buspirate_common}
                ${buspirate_nandflash}
                ${buspirate_rtt}
                platform/bpi5-rev10.h
                platform/bpi5-rev10.c
                pirate/shift.h
                pirate/shift.c
                display/robot5x16.h
                )
        target_compile_definitions(bus_pirate5_rev10 PUBLIC BP_VER=5)                
        target_compile_definitions(bus_pirate5_rev10 PUBLIC BP_REV=10)
        target_compile_definitions(bus_pirate5_rev10 PUBLIC BP_FIRMWARE_HASH="${BP_FIRMWARE_HASH}")
        if(DEFINED BP_PENDANTIC)
        target_compile_options(    bus_pirate5_rev10 PRIVATE -Wpedantic -Wall -Werror -O3 -Wno-unknown-pragmas -Wno-inline -Wno-unused-function)
        endif()

        add_executable(bus_pirate5_rev8
                ${buspirate_common}
                ${buspirate_rtt}
                platform/bpi5-rev8.h
                platform/bpi5-rev8.c
                #tf card access control
                fatfs/tf_card.c
                fatfs/tf_card.h
                pirate/shift.h
                pirate/shift.c
                display/robot5x16.h
                )
        target_compile_definitions(bus_pirate5_rev8 PUBLIC BP_VER=5) 
        target_compile_definitions(bus_pirate5_rev8 PUBLIC BP_REV=8)
        target_compile_definitions(bus_pirate5_rev8 PUBLIC BP_FIRMWARE_HASH="${BP_FIRMWARE_HASH}")
        if(DEFINED BP_PENDANTIC)
        target_compile_options(    bus_pirate5_rev8 PRIVATE -Wpedantic -Wall -Werror -O3 -Wno-unknown-pragmas -Wno-inline -Wno-unused-function)
        endif()
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
        pico_generate_pio_header(${revision} ${CMAKE_CURRENT_LIST_DIR}/pirate/hw3wire.pio)
        pico_generate_pio_header(${revision} ${CMAKE_CURRENT_LIST_DIR}/pirate/hwi2c.pio)
        pico_generate_pio_header(${revision} ${CMAKE_CURRENT_LIST_DIR}/pirate/ws2812.pio)
        pico_generate_pio_header(${revision} ${CMAKE_CURRENT_LIST_DIR}/pirate/apa102.pio) 
        pico_generate_pio_header(${revision} ${CMAKE_CURRENT_LIST_DIR}/pirate/hw1wire.pio) 
        pico_generate_pio_header(${revision} ${CMAKE_CURRENT_LIST_DIR}/pirate/hwuart.pio)  
        pico_generate_pio_header(${revision} ${CMAKE_CURRENT_LIST_DIR}/pirate/spisnif.pio)  
        pico_generate_pio_header(${revision} ${CMAKE_CURRENT_LIST_DIR}/binmode/logicanalyzer.pio)
        pico_generate_pio_header(${revision} ${CMAKE_CURRENT_LIST_DIR}/lib/pico_ir_nec/nec_carrier_burst.pio)
        pico_generate_pio_header(${revision} ${CMAKE_CURRENT_LIST_DIR}/lib/pico_ir_nec/nec_carrier_control.pio)
        pico_generate_pio_header(${revision} ${CMAKE_CURRENT_LIST_DIR}/lib/pico_ir_nec/nec_receive.pio)
        pico_generate_pio_header(${revision} ${CMAKE_CURRENT_LIST_DIR}/pirate/rc5.pio)
        pico_generate_pio_header(${revision} ${CMAKE_CURRENT_LIST_DIR}/lib/picorvd/ch32vswio.pio)
        pico_generate_pio_header(${revision} ${CMAKE_CURRENT_LIST_DIR}/pirate/pwm.pio)
        pico_generate_pio_header(${revision} ${CMAKE_CURRENT_LIST_DIR}/pirate/onewire_library.pio)
        #pico_generate_pio_header(${revision} ${CMAKE_CURRENT_LIST_DIR}/lib/pio_pwm/pwm.pio)
        pico_generate_pio_header(${revision} ${CMAKE_CURRENT_LIST_DIR}/pirate/irio.pio)
        pico_generate_pio_header(${revision} ${CMAKE_CURRENT_LIST_DIR}/commands/uart/glitch.pio)
        pico_generate_pio_header(${revision} ${CMAKE_CURRENT_LIST_DIR}/lib/pico-i2c-sniff/i2c_sniffer.pio)
        pico_generate_pio_header(${revision} ${CMAKE_CURRENT_LIST_DIR}/lib/pico-i2c-sniff/hw2w_sniffer.pio)
        pico_generate_pio_header(${revision} ${CMAKE_CURRENT_LIST_DIR}/mode/i2s_out.pio)
        pico_generate_pio_header(${revision} ${CMAKE_CURRENT_LIST_DIR}/mode/i2s_in.pio)

        pico_set_program_name(${revision} ${revision}) 
        pico_set_program_version(${revision} "0.1.0")
        if(${PICO_PLATFORM} MATCHES "rp2350")
                pico_set_program_description(${revision} "Bus Pirate 5XL/6/7 Firmware for RP2350")
        else()
                pico_set_program_description(${revision} "Bus Pirate 5 Firmware for RP2040")
        endif()
        pico_enable_stdio_uart(${revision} 0)
        pico_enable_stdio_usb(${revision} 0)

        #XOSC fix?
        target_compile_definitions(${revision} PUBLIC PICO_XOSC_STARTUP_DELAY_MULTIPLIER=172)
        # useful for debug builds
        #target_compile_definitions(${revision} PUBLIC PICO_USE_STACK_GUARDS=$<CONFIG:DEBUG>)
        target_compile_definitions(${revision} PUBLIC PICO_USE_STACK_GUARDS=1)
        target_compile_definitions(${revision} PUBLIC PICO_STACK_SIZE=4096)        
        target_compile_definitions(${revision} PRIVATE PICO_MALLOC_PANIC=0)

        # Add to your foreach(revision ${revisions}) loop after target_link_libraries
        target_link_options(${revision} PRIVATE 
        "LINKER:-Map=${CMAKE_BINARY_DIR}/${revision}.map"
        "LINKER:--print-memory-usage"
        "LINKER:--cref"
        )

        # Add the standard library to the build
        target_link_libraries(${revision} ${stdlibs} )
        target_include_directories(${revision} PUBLIC
                ${CMAKE_CURRENT_LIST_DIR}
                .
        )
        pico_add_extra_outputs(${revision})
        if(${PICO_PLATFORM} MATCHES "rp2350")
                pico_set_linker_script(${revision} ${CMAKE_CURRENT_LIST_DIR}/boards/memmap_default_rp2350.ld)
        else()
                pico_set_linker_script(${revision} ${CMAKE_CURRENT_LIST_DIR}/boards/memmap_default_rp2040.ld)
        endif()
        #pico_set_linker_script(${revision} ${CMAKE_CURRENT_LIST_DIR}/pirate/memmap_default.ld)
endforeach()

string(APPEND CMAKE_EXE_LINKER_FLAGS "-Wl,--cref,--print-memory-usage")


# by default this project uses LGPL3 libraries, if you do not want to comply
# with LGPL3 license, please set the following variable to FALSE, otherwise
# you are oblicated to distribute the resulted binaries under LGPL3 license
set(USE_LGPL3 TRUE CACHE BOOL "indicated if to include LGPL3 protected code")
include(../cmake/ansi_colours_import.cmake)
if(USE_LGPL3)
if(LEGACY_ANSI_COLOURS_ENABLED)
        if(${PICO_PLATFORM} MATCHES "rp2350")
                target_link_libraries(bus_pirate5_xl         lib_ansi_colours)
                target_link_libraries(bus_pirate6           lib_ansi_colours)
#                target_link_libraries(bus_pirate7           lib_ansi_colours)
        else()
                target_link_libraries(bus_pirate5_rev10       lib_ansi_colours)
                target_link_libraries(bus_pirate5_rev8        lib_ansi_colours)
        endif()
endif()
endif()

 
