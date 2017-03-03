all: echo

SEASTAR="./seastar"
CXXFLAGS+=$(shell /usr/bin/pkg-config --cflags ./seastar/build/release/seastar.pc)
LDFLAGS+=$(shell /usr/bin/pkg-config --libs ./seastar/build/release/seastar.pc)
CXX=g++-4.9
LD=gold

