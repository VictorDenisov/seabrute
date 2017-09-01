#ifndef __LISTENER_HPP__
#define __LISTENER_HPP__

#include <net/api.hh>

namespace seabrute {

class app;

class listener : public std::enable_shared_from_this<listener> {
    unsigned int core;
    server_socket ss;
public:
    listener(server_socket &&_ss);
    future<> accept_loop(app *_app, unsigned int core);
    future<> close();
};

}

#endif // __LISTENER_HPP__
