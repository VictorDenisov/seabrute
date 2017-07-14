#ifndef __RESULT_HPP__
#define __RESULT_HPP__

#include <core/temporary_buffer.hh>
#include "legacy.hpp"

namespace seabrute {

struct result : result_t {
    static const result deserialize(temporary_buffer<char> &&buf);
};

}

#endif // __RESULT_HPP__
