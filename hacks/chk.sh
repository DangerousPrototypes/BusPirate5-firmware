#!/bin/bash

# update CMake for both rp2040 builds and rp2350 builds
cmake -S . -B build_rp2040 -DPICO_SDK_FETCH_FROM_GIT=TRUE
cmake -S . -B build_rp2350 -DPICO_SDK_FETCH_FROM_GIT=TRUE -DBP_PICO_PLATFORM=rp2350

# optionally, do a clean build each time
cmake --build ./build_rp2040 --parallel --target clean
cmake --build ./build_rp2350 --parallel --target clean

# optionally, update the translation files
# TODO

# build everything
cmake --build ./build_rp2040 --parallel --target all
cmake --build ./build_rp2350 --parallel --target all

