#include <arpa/inet.h>
#include <iostream>
#include <cstring>
#include <util/log.hh>
#include "task.hpp"
#include "legacy.hpp"

static seastar::logger logger("task");

namespace seabrute {

task::task(std::string alph, int from, int to, std::string hash, std::string password) noexcept :
    alph(std::make_shared<std::string>(alph)),
    hash(std::make_shared<std::string>(hash)),
    from(from), to(to), password(password) {}

using seastar::temporary_buffer;

temporary_buffer<char> task::serialize() {
    task_t task;
    password.copy(task.result.password, sizeof(task.result.password));
    task.len = password.length();
    task.result.password[task.len] = 0;
    task.from = from;
    task.to = to;
    task.brute_mode = BM_ITER;
    uint32_t sz = sizeof(task) + alph->length() + hash->length() + 2;
    temporary_buffer<char> buf(sz + sizeof(sz));
    auto pos = buf.get_write();
    logger.trace("Serializing message of size {} at address {}", sz, (void*)pos);
    sz = htonl(sz);
    std::memcpy(pos, &sz, sizeof(sz));
    pos += sizeof(sz);
    std::memcpy(pos, &task, sizeof(task));
    pos += sizeof(task);
    alph->copy(pos, alph->length());
    pos += alph->length();
    *pos++ = 0;
    hash->copy(pos, hash->length());
    pos += hash->length();
    *pos++ = 0;
    logger.trace("Finished serializing message at address {}", (void*)pos);
    return buf;
}

}
