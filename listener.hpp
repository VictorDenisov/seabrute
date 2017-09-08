#ifndef __LISTENER_HPP__
#define __LISTENER_HPP__

#include <net/api.hh>

namespace seabrute {

class app;

class listener : public std::enable_shared_from_this<listener> {
    server_socket ss;
    unsigned int core;
public:
    listener(server_socket &&_ss, unsigned int core);
    future<> accept_loop(app *_app);
    future<> close();
};

}

#endif // __LISTENER_HPP__
