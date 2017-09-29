#ifndef __LISTENER_HPP__
#define __LISTENER_HPP__

#include <net/api.hh>

namespace seabrute {

using seastar::future;

class app;

class listener {
    seastar::server_socket ss;
    unsigned int core;
public:
    listener(seastar::server_socket &&_ss, unsigned int core);
    future<> accept_loop(app *_app);
    future<> close();
};

}

#endif // __LISTENER_HPP__
