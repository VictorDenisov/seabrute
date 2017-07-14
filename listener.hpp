#ifndef __LISTENER_HPP__
#define __LISTENER_HPP__

#include <net/api.hh>

namespace seabrute {

class app;

class listener : public std::enable_shared_from_this<listener> {
    server_socket ss;
public:
    listener(server_socket &&_ss);
    future<> accept_loop(app *_app);
    void close();
};

}

#endif // __LISTENER_HPP__
