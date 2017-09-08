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
    logger.debug("Listen on core {}", core);
    auto list = listen(make_ipv4_address({1234}), lo);
    logger.debug("Creating socket on core {}", core);
    auto ss = server_socket(std::move(list));
    if (closing) {
        logger.debug("App is already closing, not creating socket on core ", core);
        return make_ready_future<>();
    }
    return add_listener(std::move(ss), core)
    .then([this, core] (std::shared_ptr<listener> l) mutable {
        return l->accept_loop(this);
    });
}

future<std::shared_ptr<listener>> app::add_listener(server_socket &&ss, unsigned int core) {
    logger.debug("Adding listener..");
    return smp::submit_to(0, [this, ss = std::move(ss), core] () mutable {
        auto ptr = self_deleting_weak_ref<listener>::create(listeners, ss, core);
        logger.debug("Added listener ", ptr);
        return make_ready_future<std::shared_ptr<listener>>(ptr);
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
        logger.debug("Closing app {} on CPU #0", this);
        return parallel_for_each(listeners, [] (auto weak_listener) {
            logger.debug("Going to close listener by ref {}", weak_listener);
            auto listener = weak_listener->lock();
            if (listener) {
                return listener->close();
            }
            return make_ready_future<>();
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
            logger.debug("Starting listener on core {}", core);
            return smp::submit_to(core, [this, core] () mutable {
                logger.debug("Starting main_async on core {}", core);
                return main_async(core);
            });
        });
    });
}

}
