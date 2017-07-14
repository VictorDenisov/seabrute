all: seabrute
seabrute: app.o config.o listener.o main.o result.o server_connection.o task.o task_generator.o
	$(LINK.o) $^ $(LOADLIBES) $(LDLIBS) -o $@

SEASTAR="./seastar"
CXXFLAGS+=$(shell /usr/bin/pkg-config --cflags ./seastar/build/debug/seastar.pc)
LDFLAGS+=$(shell /usr/bin/pkg-config --libs ./seastar/build/debug/seastar.pc)
CXX=g++-6
LD=gold

