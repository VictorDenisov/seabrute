main: echo
SEASTAR="./seastar"
CXX=g++-5
LD=gold
CPPFLAGS+=$(shell pkg-config --cflags ${SEASTAR}/build/release/seastar.pc)
LDFLAGS+=$(shell pkg-config --libs ${SEASTAR}/build/release/seastar.pc)
