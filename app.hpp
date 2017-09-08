#ifndef __APP_HPP__
#define __APP_HPP__

#include <core/app-template.hh>
#include <net/api.hh>
#include "listener.hpp"
#include "self_deleting_weak_ref.hpp"
#include "task.hpp"
#include "task_generator.hpp"

namespace seabrute {

class app : public app_template {
    std::shared_ptr<seabrute::task_generator> tsk_gen;
    typename self_deleting_weak_ref<listener>::container_t listeners;
    bool closing = false;

    future<> main_async(unsigned int core);
    future<std::shared_ptr<listener>> add_listener(server_socket &&ss, unsigned int core);

public:
    app();
    bool is_closing();
    future<task> get_next_task();
    future<> close();
    int run(int ac, char ** av);
};

}

#endif // __APP_HPP__
