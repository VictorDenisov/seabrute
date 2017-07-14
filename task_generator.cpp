#include <iostream>
#include "task_generator.hpp"

namespace seabrute {

task_generator* task_generator::original_address = nullptr;

task_generator::task_generator(task &&task) : current(task), index(task.to - task.from), started(false), finished(false) {
    int len = current.to - current.from;
    if (len == 0)
        finished = true;
    current.password.replace(current.from, len, len, (*current.alph)[0]);
    std::cerr << "Task generator address " << (void*)this << std::endl;
    original_address = this;
}

task task_generator::get_next() {
    std::cerr << "Task generator address in get_next " << (void*)this << std::endl;
    assert (this == original_address);
    if (finished)
        throw generator_finished();
    if (not started) {
        started = true;
        return current;
    }

    auto pass_shift = current.password.length() - current.to;
    auto pos = current.password.rbegin() + pass_shift;
    for (auto i = index.rbegin(); i != index.rend(); ++i, ++pos) {
        if (*i == current.alph->length() - 1) {
            *i = 0;
            *pos = (*current.alph)[0];
        } else {
            ++*i;
            *pos = (*current.alph)[*i];
            return current;
        }
    }
    finished = true;
    throw generator_finished();
}

}
