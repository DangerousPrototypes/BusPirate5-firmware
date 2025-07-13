add_library(picomp3lib INTERFACE)
target_sources(picomp3lib INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/bitstream.c
    ${CMAKE_CURRENT_LIST_DIR}/buffers.c
    ${CMAKE_CURRENT_LIST_DIR}/dct32.c
    ${CMAKE_CURRENT_LIST_DIR}/dequant.c
    ${CMAKE_CURRENT_LIST_DIR}/dqchan.c
    ${CMAKE_CURRENT_LIST_DIR}/huffman.c
    ${CMAKE_CURRENT_LIST_DIR}/hufftabs.c
    ${CMAKE_CURRENT_LIST_DIR}/imdct.c
    ${CMAKE_CURRENT_LIST_DIR}/mp3dec.c
    ${CMAKE_CURRENT_LIST_DIR}/mp3tabs.c
    ${CMAKE_CURRENT_LIST_DIR}/polyphase.c
    ${CMAKE_CURRENT_LIST_DIR}/scalfact.c
    ${CMAKE_CURRENT_LIST_DIR}/stproc.c
    ${CMAKE_CURRENT_LIST_DIR}/subband.c
    ${CMAKE_CURRENT_LIST_DIR}/trigtabs.c
)
target_include_directories(picomp3lib INTERFACE
    .
    ../interface
)
target_link_libraries(picomp3lib INTERFACE
    pico_stdlib
)
