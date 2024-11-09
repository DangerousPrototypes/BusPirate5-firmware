# == DO NEVER EDIT THE NEXT LINES for Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
if(WIN32)
set(sdkVersion 2.0.0)
set(toolchainVersion 13_2_Rel1)
set(picotoolVersion 2.0.0)
include(${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
endif()
# ====================================================================================
cmake_minimum_required(VERSION 3.13)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17) 

# initalize pico_sdk from installed location
# (note this can come from environment, CMake cache etc)
#set(PICO_SDK_PATH "C:/pico113/pico-sdk")

# Add my_pico_w.h
set(PICO_BOARD_HEADER_DIRS ${CMAKE_CURRENT_LIST_DIR}/src/boards ) 
# Add my_pico_w.cmake
set(PICO_BOARD_CMAKE_DIRS ${CMAKE_CURRENT_LIST_DIR}/src/boards )
# Add my_pico_w
#

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


add_subdirectory(src)
