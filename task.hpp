#ifndef __TASK_HPP__
#define __TASK_HPP__

#include <core/temporary_buffer.hh>

namespace seabrute {

struct task {
    std::shared_ptr<std::string> alph, hash;
    int from, to;
    std::string password;
    task(std::string alph, int from, int to, std::string hash, std::string password) noexcept;
    temporary_buffer<char> serialize();
};

}

#endif // __TASK_HPP__
