#ifndef __TASK_GENERATOR_HPP__
#define __TASK_GENERATOR_HPP__

#include <vector>
#include "task.hpp"

namespace seabrute {

class task_generator {
    static task_generator* original_address;
    task current;
    std::vector<unsigned int> index;
    bool started, finished;
public:
    class generator_finished : std::exception {
        virtual const char* what() const noexcept {
            return "Generator finished";
        }
    };
    task_generator(task &&task);
    task get_next();
};

}

#endif // __TASK_GENERATOR_HPP__
