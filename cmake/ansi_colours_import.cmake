# Author: Lior Shalmay
# Description: This files imports the project "ansi_colours" (located at https://github.com/mina86/ansi_colours.git).
#              That project helps converting RGB colors to ANSI 256 colors, in order to support non-trurcolor terminals.
# Copyright (c) 2024 Lior Shalmay <liorshalmay1@gmail.com> 
#
# Note: The ansi_colours project is distributed under LGPL3. If you don't want to
#       include that license then disable ANSI 256 color support (enabled by default).
#       For further license explanation, please look at the docs/licenses.md file.
# copyright notice:
# the used ansi_colours code (ansi256.c ansi_colours.h) is copyrighted by the following people:
#        Michał Nazarewicz <mina86@mina86.com> 2018

set(ANSI_COLOURS_GIT_URL "https://github.com/mina86/ansi_colours.git")
set(ANSI_COLOURS_GIT_TAG "v1.2.2")
set(ANSI_COLOURS_PATH "" CACHE PATH "Path to the ansi_colours library's source code")

set(LEGACY_ANSI_COLOURS_ENABLED TRUE CACHE BOOL "Chooses to include the ansi_colours project or not to include it")

if(USE_LGPL3)
if(LEGACY_ANSI_COLOURS_ENABLED)
    if(NOT ANSI_COLOURS_PATH)
        include(FetchContent)
        set(FETCHCONTENT_BASE_DIR_SAVE ${FETCHCONTENT_BASE_DIR})
        FetchContent_Declare(
            ansi_colours
            GIT_REPOSITORY ${ANSI_COLOURS_GIT_URL}
            GIT_TAG ${ANSI_COLOURS_GIT_TAG}
        )
        FetchContent_Populate(ansi_colours)
        set(ANSI_COLOURS_PATH ${ansi_colours_SOURCE_DIR} CACHE PATH "Path to the ansi_colours library's source code" FORCE)
    endif()
    add_compile_definitions(ANSI_COLOR_256)
    include_directories(AFTER ${ANSI_COLOURS_PATH}/src/)
    add_library(lib_ansi_colours STATIC ${ANSI_COLOURS_PATH}/src/ansi256.c)
endif()
endif()
