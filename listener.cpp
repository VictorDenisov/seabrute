#include <util/log.hh>
#include "app.hpp"
#include "listener.hpp"
#include "server_connection.hpp"

static seastar::logger logger("listener");

namespace seabrute {

listener::listener(server_socket &&_ss) : ss(std::move(_ss)) {}

future<> listener::accept_loop(app *_app) {
    auto sthis = shared_from_this();
    return repeat([sthis, _app] () mutable {
        logger.debug("Starting accept loop for listener {}", sthis); 
        if (_app->is_closing()) {
            logger.debug("App is closing, get out of here {}", sthis);
            return make_ready_future<stop_iteration>(stop_iteration::yes);
        }
        return sthis->ss.accept().then([sthis, _app] (connected_socket s, socket_address a) mutable {
            logger.debug("Accepted connection from {} in listener {}", a, sthis);
            return do_with(server_connection(std::move(s)), [_app] (server_connection &sc) {
                return sc.life_cycle(_app);
            });
        }).then([] {
            return make_ready_future<stop_iteration>(stop_iteration::no);
        });
    });
}

void listener::close() {
    logger.debug("Closing listener {}", this);
    ss.abort_accept();
}

}
