#!/bin/bash

# exit immediately if a command exits with a non-zero status
#     -e exits on error
#     -u errors on undefined variables
#     -x prints commands before execution
#     -o (for option) pipefail exits on command pipe failures
# Some gotchas and workarounds are documented well at following:
#     https://vaneyckt.io/posts/safer_bash_scripts_with_set_euxo_pipefail/
#
# The shell does **_NOT_** exit if the command that fails is:
#     * part of the command list immediately following a `while` or `until` keyword;
#     * part of the test following the `if` or `elif` reserved words;
#     * part of any command executed in a `&&` or `||` list, except the command following the final `&&` or `||`;
#     * any command in a pipeline, except the last command in the pipeline; or
#     * if the command's return value is being inverted with `!`
set -euxo pipefail

# First version:
# * Current working directory must be depot root.
# * Run as `./hacks/chk.sh`
# TODO:
# * [ ] Automatically set proper working directory (depot root, parent of script directory)
# * [ ] Add cmd-line option `--clean` to do clean build
# * [ ] Add cmd-line option `--translations` to generate updated translation files
# * [ ] Detect errors and stop script when detected
# * [x] Maybe set bash to exit on errors ... `set -e`?

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


