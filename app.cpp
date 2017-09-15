#include <boost/range/irange.hpp>
#include <core/reactor.hh>
#include <iostream>
#include <util/log.hh>
#include "app.hpp"
#include "config.hpp"
#include "listener.hpp"

static seastar::logger logger("app");

namespace seabrute {

future<> app::main_async(unsigned int core) {
    listen_options lo;
    lo.reuse_address = true;
    logger.trace("Listen on core {}", core);
    auto ss = listen(make_ipv4_address({1234}), lo);
    if (closing) {
        logger.debug("App is already closing, not creating socket on core ", core);
        return make_ready_future<>();
    }
    return add_listener(std::move(ss), core)
    .then([this, core] (listener_ptr l) mutable {
        return l->accept_loop(this)
        .finally([this, l] {
            listeners.erase(l);
            return make_ready_future<>();
        });
    });
}

future<listener_ptr> app::add_listener(server_socket &&ss, unsigned int core) {
    logger.trace("Adding listener..");
    return smp::submit_to(0, [this, ss = std::move(ss), core] () mutable {
        auto pos = listeners.insert(listeners.begin(), listener(std::move(ss), core));
        return make_ready_future<listener_ptr>(pos);
    });
}

app::app() {
    config::register_options(add_options());
}

bool app::is_closing() {
    return closing;
}

future<task> app::get_next_task() {
    return smp::submit_to(0, [this] () mutable {
        return tsk_gen->get_next();
    });
}

future<> app::close() {
    logger.debug("Closing app {}", this);
    closing = true;
    return smp::submit_to(0, [this] () mutable {
        logger.trace("Closing app {} on CPU #0", this);
        return parallel_for_each(listeners, [] (listener &listener) {
            return listener.close();
        }).then([this] () mutable {
            return seastar::when_all_succeed(main_asyncs_done_promise.get_future());
        });
    });
}

int app::run(int ac, char ** av) {
    return app_template::run(ac, av, [this] {
        seastar::logger_registry().set_all_loggers_level(seastar::log_level::debug);
        auto config = seabrute::config(configuration());
        tsk_gen = std::make_shared<seabrute::task_generator>(seabrute::task(config.alph, 0, config.length, config.hash, std::string(config.length, '\0')));
        engine().at_exit(std::bind(&app::close, this));
        logger.debug("Will start {} listeners", smp::count);
        return parallel_for_each(boost::irange<unsigned int>(0, smp::count), [this] (unsigned int core) mutable {
            logger.trace("Starting listener on core {}", core);
            return smp::submit_to(core, [this, core] () mutable {
                logger.trace("Starting main_async on core {}", core);
                return main_async(core);
            });
        }).finally([this] () mutable {
            main_asyncs_done_promise.set_value();
        });
    });
}

}
