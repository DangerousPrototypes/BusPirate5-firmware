# Author: Lior Shalmay
# Description: This files imports the project "ansi_colours" (located at https://github.com/mina86/ansi_colours.git).
#              That project helps converting RGB colors to ANSI 256 colors, in order to support non-trurcolor terminals.
# Copyright (c) 2024 Lior Shalmay <liorshalmay1@gmail.com> 
#
# Note: The ansi_colours project is distributed under LGPL3, if you dont want to
#       include that license than disable ANSI 256 color support (disabled by default).
#       For farther license explaination please look at file License compliance is explained at docs/licenses.md
# copyright notice:
# the used ansi_colours code (ansi256.c ansi_colours.h) is copyrighted by the following people:
#        Michał Nazarewicz <mina86@mina86.com> 2018

set(ANSI_COLOURS_GIT_URL "https://github.com/mina86/ansi_colours.git")
set(ANSI_COLOURS_GIT_TAG "v1.2.2")
set(ANSI_COLOURS_PATH "" CACHE PATH "Path to the ansi_colours library's source code")

# Note: by changing this value to TRUE, you are agreeing to the terms of use of the LGPL3 license,
# and thus applying the LGPL3 license to the produced binaries and to this entire code base.
set(LEGACY_ANSI_COLOURS_ENABLED FALSE CACHE BOOL "Chooses to include the ansi)colours project or not to include it")

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
