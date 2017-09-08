#include <core/reactor.hh>
#include <util/log.hh>
#include "app.hpp"
#include "listener.hpp"
#include "server_connection.hpp"

static seastar::logger logger("listener");

namespace seabrute {

listener::listener(server_socket &&_ss, unsigned int core) : ss(std::move(_ss)), core(core) {}

future<> listener::accept_loop(app *_app) {
    auto sthis = shared_from_this();
    return repeat([sthis, _app] () mutable {
        logger.trace("Starting accept loop for listener {}", sthis); 
        if (_app->is_closing()) {
            logger.trace("App is closing, get out of here {}", sthis);
            return make_ready_future<stop_iteration>(stop_iteration::yes);
        }
        return sthis->ss.accept().then([sthis, _app] (connected_socket s, socket_address a) mutable {
            logger.trace("Accepted connection from {} in listener {}", a, sthis);
            return do_with(server_connection(std::move(s)), [_app] (server_connection &sc) {
                return sc.life_cycle(_app);
            });
        }).then([] {
            return make_ready_future<stop_iteration>(stop_iteration::no);
        }).handle_exception([] (std::exception_ptr e_ptr) {
            try {
                std::rethrow_exception(e_ptr);
            } catch (const std::exception &e) {
                logger.info("Got exception in listener loop, exiting: {}", e.what());
            }
            return make_ready_future<stop_iteration>(stop_iteration::yes);
        });
    });
}

future<> listener::close() {
    logger.trace("Closing listener {}", this);
    return smp::submit_to(core, [this] () mutable {
        ss.abort_accept();
        return make_ready_future<>();
    });
}

}
