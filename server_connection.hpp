#ifndef __SERVER_CONNECTION_HPP__
#define __SERVER_CONNECTION_HPP__

#include <net/api.hh>
#include "app.hpp"

namespace seabrute {

class server_connection {
    connected_socket cs;
    input_stream<char> input;
    output_stream<char> output;

    future<> read_cycle(app *_app);
    future<> send_next_task(app *_app);
public:
    server_connection(connected_socket &&_cs);
    future<> life_cycle(app *_app);
};

}

#endif // __SERVER_CONNECTION_HPP__
