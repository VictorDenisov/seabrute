#include "core/app-template.hh"
#include "core/seastar.hh"
#include "core/reactor.hh"
#include "core/future-util.hh"
#include <boost/range/irange.hpp>
#include <iostream>
#include <vector>
#include <string>
#include <arpa/inet.h>

namespace seabrute {

#include "legacy.hpp"

/****** Config ******/

namespace bpo = boost::program_options;

struct config {
    std::string alph;
    std::string hash;
    int length;

    config(bpo::variables_map &args) :
        alph(args["alph"].as< std::string >()),
        hash(args["hash"].as< std::string >()),
        length(args["length"].as<int>()) {}

    static void register_options(bpo::options_description_easy_init &&add_options) {
        add_options
            ("alph,a", bpo::value< std::string >()->default_value(default_alph), "alphabet")
            ("hash,h", bpo::value< std::string >()->default_value(default_hash), "hash")
            ("length,n", bpo::value<int>()->default_value(default_length), "password length")
            ;
    }

private:
    static const std::string default_alph;
    static const std::string default_hash;
    static const int default_length;
};

//const std::string config::default_alph = std::string("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789");
const std::string config::default_alph = std::string("csit");
const std::string config::default_hash = std::string("eMWCfUJDk9Lec");
const int config::default_length = 4;

/********************/

struct task {
    std::shared_ptr<std::string> alph, hash;
    int from, to;
    std::string password;
    task(std::string alph, int from, int to, std::string hash, std::string password) noexcept :
        alph(std::make_shared<std::string>(alph)),
        hash(std::make_shared<std::string>(hash)),
        from(from), to(to), password(password) {}
    temporary_buffer<char> serialize() {
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
        sz = htonl(sz);
        memcpy(pos, &sz, sizeof(sz));
        pos += sizeof(sz);
        memcpy(pos, &task, sizeof(task));
        pos += sizeof(task);
        alph->copy(pos, alph->length());
        pos += alph->length();
        *pos++ = 0;
        hash->copy(pos, hash->length());
        pos += hash->length();
        *pos++ = 0;
        return buf;
    }
};

class task_generator {
    task current;
    std::vector<unsigned int> index;
    bool started, finished;
public:
    task_generator(task &&task) : current(task), index(task.to - task.from), started(false), finished(false) {
        int len = current.to - current.from;
        if (len == 0)
            finished = true;
        current.password.replace(current.from, len, len, (*current.alph)[0]);
    }
    boost::optional<task> get_next() {
        if (finished)
            return boost::none;
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
        return boost::none;
    }
};

using unconsumed_remainder = std::experimental::optional<temporary_buffer<char>>;

struct consumer {
    std::shared_ptr<task_generator> tsk_gen;
    output_stream<char> *output;

    consumer(std::shared_ptr<task_generator> _tsk_gen, output_stream<char> *_output) : tsk_gen(_tsk_gen), output(_output) {
    }

    future<unconsumed_remainder> operator()(temporary_buffer<char> buf) {
        if (buf) {
            return smp::submit_to(0, [&] {
                return tsk_gen->get_next();
            }).then([&] (boost::optional<task> ot) {
                return;
            }).then([] () {
                return unconsumed_remainder();
            });
        } else {
            return make_ready_future<unconsumed_remainder>(unconsumed_remainder());
        }
    }
};

future<>
handle_connection (std::shared_ptr<task_generator> tsk_gen, connected_socket s, socket_address a) {
    input_stream<char> input = s.input();
    output_stream<char> output = s.output();
    return do_with(std::move(input), std::move(output), [tsk_gen] (input_stream<char> &input, output_stream<char> &output) {
        /* send all 4 tasks, then in consumer parse one result and send one task */
        auto range = boost::irange(0, 4);
        auto c = consumer(tsk_gen, &output);
        return do_for_each(range, [&output, &tsk_gen] (int) {
            return smp::submit_to(0, [&] {
                return tsk_gen->get_next();
            }).then([&output] (boost::optional<task> ot) {
                if (ot) {
                    return output.write(ot->serialize());
                } else {
                    return output.close();  /* we should handle this better, mb raise an exception */
                }
            });
        }).then([&output] () {
            return output.flush();
        }).then([c{std::move(c)}, &input] {
            return do_with(std::move(c), std::move(input), [] (auto &c, auto &input) {
                return input.consume(c);
            });
        });
    });
}

future<>
main_async(std::shared_ptr<task_generator> tsk_gen) {
    listen_options lo;
    lo.reuse_address = true;
    return do_with(listen(make_ipv4_address({1234}), lo), [tsk_gen] (auto& listener) {
        return keep_doing([&listener, tsk_gen] () {
            return listener.accept().then([tsk_gen] (connected_socket s, socket_address a) {
                handle_connection(tsk_gen, std::move(s), std::move(a));
            });
        });
    });
}

} /* namespace seabrute */

int main(int argc, char** argv) {
    app_template app;
    namespace bpo = boost::program_options;
    seabrute::config::register_options(app.add_options());
    try {
        app.run(argc, argv, [&app] {
            auto config = seabrute::config(app.configuration());
            auto tsk_gen = std::make_shared<seabrute::task_generator>(seabrute::task(config.alph, 0, config.length, config.hash, std::string(config.length, '\0')));
            return parallel_for_each(boost::irange<unsigned int>(0, smp::count), [tsk_gen] (unsigned int core) {
                return smp::submit_to(core, [tsk_gen] {return main_async(tsk_gen);});
            });
        });
    } catch(std::runtime_error &e) {
        std::cerr << "Couldn't start application: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
