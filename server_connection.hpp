#ifndef __SERVER_CONNECTION_HPP__
#define __SERVER_CONNECTION_HPP__

#include <net/api.hh>
#include "app.hpp"

namespace seabrute {

using seastar::connected_socket;
using seastar::future;

class server_connection {
    connected_socket cs;
    seastar::input_stream<char> input;
    seastar::output_stream<char> output;

    future<> read_cycle(app *_app);
    future<> send_next_task(app *_app);
public:
    server_connection(connected_socket &&_cs);
    future<> life_cycle(app *_app);
};

}

#endif // __SERVER_CONNECTION_HPP__
