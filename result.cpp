#include "result.hpp"

const seabrute::result seabrute::result::deserialize(seastar::temporary_buffer<char> &&buf) {
    assert(buf.size() == sizeof(result));
    return *reinterpret_cast<const result*>(buf.get());
}
