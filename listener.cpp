#include <core/reactor.hh>
#include <util/log.hh>
#include "app.hpp"
#include "listener.hpp"
#include "server_connection.hpp"

static seastar::logger logger("listener");

namespace seabrute {

using seastar::make_ready_future;
using seastar::stop_iteration;

listener::listener(seastar::server_socket &&_ss, unsigned int core) : ss(std::move(_ss)), core(core) {}

future<> listener::accept_loop(app *_app) {
    return seastar::repeat([this, _app] () mutable {
        logger.trace("Starting accept loop for listener {}", this); 
        if (_app->is_closing()) {
            logger.trace("App is closing, get out of here {}", this);
            return make_ready_future<stop_iteration>(stop_iteration::yes);
        }
        return ss.accept().then([this, _app] (connected_socket s, seastar::socket_address a) mutable {
            logger.trace("Accepted connection from {} in listener {}", a, this);
            return seastar::do_with(server_connection(std::move(s)), [_app] (server_connection &sc) {
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
    return seastar::smp::submit_to(core, [this] () mutable {
        ss.abort_accept();
        return make_ready_future<>();
    });
}

}
