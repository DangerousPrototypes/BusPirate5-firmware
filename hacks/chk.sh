#!/bin/bash

# First version:
# * Current working directory must be depot root.
# * Run as `./hacks/chk.sh`
# TODO:
# * [ ] Automatically set proper working directory (depot root, parent of script directory)
# * [ ] Add cmd-line option `--clean` to do clean build
# * [ ] Add cmd-line option `--translations` to generate updated translation files
# * [ ] Detect errors and stop script when detected
# * [ ] Maybe set bash to exit on errors ... `set -e`?

# update CMake for both rp2040 builds and rp2350 builds
cmake -S . -B build_rp2040 -DPICO_SDK_FETCH_FROM_GIT=TRUE
cmake -S . -B build_rp2350 -DPICO_SDK_FETCH_FROM_GIT=TRUE -DBP_PICO_PLATFORM=rp2350

# optionally, do a clean build each time
cmake --build ./build_rp2040 --parallel --target clean
cmake --build ./build_rp2350 --parallel --target clean

# optionally, update the translation files
# TODO: Run `./src/translation/json2h.py` to generate updated translation files
python ./src/translation/json2h.py

# build everything
cmake --build ./build_rp2040 --parallel --target all
cmake --build ./build_rp2350 --parallel --target all

