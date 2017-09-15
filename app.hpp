#ifndef __APP_HPP__
#define __APP_HPP__

#include <core/app-template.hh>
#include <list>
#include <net/api.hh>
#include "listener.hpp"
#include "task.hpp"
#include "task_generator.hpp"

namespace seabrute {

using listener_ptr = std::list<listener>::iterator;

class app : public app_template {
    std::shared_ptr<seabrute::task_generator> tsk_gen;
    std::list<listener> listeners;
    bool closing = false;
    promise<> main_asyncs_done_promise;

    future<> main_async(unsigned int core);
    future<listener_ptr> add_listener(server_socket &&ss, unsigned int core);

public:
    app();
    bool is_closing();
    future<task> get_next_task();
    future<> close();
    int run(int ac, char ** av);
};

}

#endif // __APP_HPP__
