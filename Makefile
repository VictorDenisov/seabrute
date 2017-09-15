all: seabrute
seabrute: app.o config.o listener.o main.o result.o server_connection.o task.o task_generator.o
	$(LINK.o) $^ $(LOADLIBES) $(LDLIBS) -o $@

SEASTAR="./seastar"
CXXFLAGS+=$(shell /usr/bin/pkg-config --cflags ./seastar/build/debug/seastar.pc)
LDFLAGS+=$(shell /usr/bin/pkg-config --libs ./seastar/build/debug/seastar.pc)
CXX=g++-6
LD=gold

app.o: app.cpp app.hpp config.hpp listener.hpp
config.o: config.cpp config.hpp
listener.o: listener.cpp app.hpp listener.hpp server_connection.hpp
main.o: main.cpp app.hpp
result.o: result.cpp result.hpp
server_connection.o: server_connection.cpp result.hpp server_connection.hpp
task.o: task.cpp task.hpp legacy.hpp
task_generator.o: task_generator.cpp task_generator.hpp
