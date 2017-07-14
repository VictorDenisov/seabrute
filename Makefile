all: echo
echo: echo.o config.o result.o task.o task_generator.o

SEASTAR="./seastar"
CXXFLAGS+=$(shell /usr/bin/pkg-config --cflags ./seastar/build/debug/seastar.pc)
LDFLAGS+=$(shell /usr/bin/pkg-config --libs ./seastar/build/debug/seastar.pc)
CXX=g++-6
LD=gold

