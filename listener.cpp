#include "app.hpp"
#include "listener.hpp"
#include "server_connection.hpp"

namespace seabrute {

listener::listener(server_socket &&_ss) : ss(std::move(_ss)) {}

future<> listener::accept_loop(app *_app) {
    auto sthis = shared_from_this();
    return repeat([sthis, _app] () mutable {
        std::cerr << "Starting accept loop for listener " << sthis << std::endl;
        if (_app->is_closing()) {
            std::cerr << "App is closing, get out of here " << sthis << std::endl;
            return make_ready_future<stop_iteration>(stop_iteration::yes);
        }
        return sthis->ss.accept().then([sthis, _app] (connected_socket s, socket_address a) mutable {
            std::cerr << "Accepted connection from " << a << " in listener " << sthis << std::endl;
            return do_with(server_connection(std::move(s)), [_app] (server_connection &sc) {
                return sc.life_cycle(_app);
            });
        }).then([] {
            return make_ready_future<stop_iteration>(stop_iteration::no);
        });
    });
}

void listener::close() {
    std::cerr << "Closing listener " << this << std::endl;
    ss.abort_accept();
}

}
