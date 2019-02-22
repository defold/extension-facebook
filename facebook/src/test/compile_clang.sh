#!/usr/bin/env bash

set -e
if [ ! -e build ]; then
    mkdir -p build
fi

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

#OPT=-O3
OPT="-g -O2"
# for debugging
#DISASSEMBLY="-S -masm=intel"
#PREPROCESS="-E"
#ASAN="-fsanitize=address -fno-omit-frame-pointer -fsanitize-address-use-after-scope -fsanitize=undefined"
#ASAN_LDFLAGS="-fsanitize=address "
CXXFLAGS="$CXXFLAGS -std=c++98 -Wall -Weverything -pedantic -Wno-global-constructors -Isrc -I${DIR}"
LDFLAGS=
ARCH=-m64

function compile_test {
    local name=$1
    clang++ -o ./build/${name} $OPT $ARCH $DISASSEMBLY $CXXFLAGS $LDFLAGS -c test/${name}.cpp
}

time compile_test test_fb

