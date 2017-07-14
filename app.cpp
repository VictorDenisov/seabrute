#include <boost/range/irange.hpp>
#include <core/reactor.hh>
#include <iostream>
#include "app.hpp"
#include "config.hpp"
#include "listener.hpp"

namespace seabrute {

future<> app::main_async(unsigned int core) {
    listen_options lo;
    lo.reuse_address = true;
    std::cerr << "Listen on core " << core << std::endl;
    auto list = listen(make_ipv4_address({1234}), lo);
    std::cerr << "Creating socket on core " << core << std::endl;
    auto ss = server_socket(std::move(list));
    if (closing) {
        std::cerr << "App is already closing, not creating socket on core " << core << std::endl;
        return make_ready_future<>();
    }
    return add_listener(std::move(ss))
    .then([this] (std::shared_ptr<listener> l) mutable {
        return l->accept_loop(this);
    });
}

future<std::shared_ptr<listener>> app::add_listener(server_socket &&ss) {
    std::cerr << "Adding listener.." << std::endl;
    return smp::submit_to(0, [this, ss = std::move(ss)] () mutable {
        auto ptr = self_deleting_weak_ref<listener>::create(listeners, ss);
        std::cerr << "Added listener " << ptr << std::endl;
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
    std::cerr << "Closing app " << this << std::endl;
    closing = true;
    return smp::submit_to(0, [this] () mutable {
        std::cerr << "Closing app " << this << " on CPU #0" << std::endl;
        for (auto weak_listener : listeners) {
            std::cerr << "Going to close listener by ref " << weak_listener << std::endl;
            auto listener = weak_listener->lock();
            if (listener) {
                listener->close();
            }
        }
        return make_ready_future<>();
    });
}

int app::run(int ac, char ** av) {
    return app_template::run(ac, av, [this] {
        auto config = seabrute::config(configuration());
        tsk_gen = std::make_shared<seabrute::task_generator>(seabrute::task(config.alph, 0, config.length, config.hash, std::string(config.length, '\0')));
        std::cerr << "Will start " << smp::count << " listeners" << std::endl;
        return parallel_for_each(boost::irange<unsigned int>(0, smp::count), [this] (unsigned int core) mutable {
            std::cerr << "Starting listener on core " << core << std::endl;
            return smp::submit_to(core, [this, core] () mutable {
                std::cerr << "Starting main_async on core " << core << std::endl;
                return main_async(core);
            });
        });
    });
}

}
