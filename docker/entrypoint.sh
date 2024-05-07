#!/bin/bash

if [ -z "$1" ]; then
    bash
elif [[ "$1" == "build-clean" ]]; then
    cd /project;
    mkdir build;
    cd build;
    rm -rf *;
    cmake ../;
    make;
else
    echo "Running $1"
    set -ex
    bash -c "$1"
fi
