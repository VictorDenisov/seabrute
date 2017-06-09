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
        std::cerr << "Serializing message of size " << sz << " at address " << (void*)pos << std::endl;
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
        std::cerr << "Finished serializing message at address " << (void*)pos << std::endl;
        return buf;
    }
};

struct result : result_t {
    static const result deserialize(temporary_buffer<char> &&buf) {
        assert(buf.size() == sizeof(result));
        return *reinterpret_cast<const result*>(buf.get());
    }
};

class task_generator {
    static task_generator* original_address;
    task current;
    std::vector<unsigned int> index;
    bool started, finished;
public:
    task_generator(task &&task) : current(task), index(task.to - task.from), started(false), finished(false) {
        int len = current.to - current.from;
        if (len == 0)
            finished = true;
        current.password.replace(current.from, len, len, (*current.alph)[0]);
        std::cerr << "Task generator address " << (void*)this << std::endl;
        original_address = this;
    }
    boost::optional<task> get_next() {
        std::cerr << "Task generator address in get_next " << (void*)this << std::endl;
        assert (this == original_address);
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
task_generator* task_generator::original_address = nullptr;

class app : public app_template {
    std::shared_ptr<seabrute::task_generator> tsk_gen;
    std::vector<std::weak_ptr<server_socket>> listeners;

    future<>
    main_async() {
        listen_options lo;
        lo.reuse_address = true;
        auto listener = std::make_shared<server_socket>(listen(make_ipv4_address({1234}), lo));
        return add_listener(listener).then([this, listener] () mutable {
            return keep_doing([this, listener] () mutable {
                return listener->accept().then([this] (connected_socket s, socket_address a) mutable {
                    handle_connection(std::move(s), std::move(a));
                });
            });
        });
    }

    future<>
    add_listener(std::shared_ptr<server_socket> listener) {
        return smp::submit_to(0, [this, listener] () mutable {
            listeners.push_back(listener);
            return make_ready_future<>();
        });
    }

    future<>
    close() {
        return smp::submit_to(0, [this] () mutable {
            for (auto weak_listener : listeners) {
                auto listener = weak_listener.lock();
                if (listener) {
                    listener->abort_accept();
                }
            }
            return make_ready_future<>();
        });
    }

    future<>
    handle_connection (connected_socket s, socket_address a) {
        input_stream<char> input = s.input();
        output_stream<char> output = s.output();
        return do_with(std::move(input), std::move(output), [this] (input_stream<char> &input, output_stream<char> &output) mutable {
            /* send all 4 tasks, then in consumer parse one result and send one task */
            auto range = boost::irange(0, 4);
            return do_for_each(range, [this, &output] (int) mutable {
                return send_next_task(output);
            }).then([this, &input, &output] () mutable {
                return read_cycle(input, output);
            });
        });
    }

    future<>
    read_cycle (input_stream<char> &input, output_stream<char> &output) {
        return repeat([this, &input, &output] () mutable {
            return input.read_exactly(sizeof(uint32_t))
            .then([&input] (temporary_buffer<char> buf) {
                if (!buf) {
                    return make_ready_future<temporary_buffer<char>>(std::move(buf));
                }
                uint32_t sz = ntohl(*reinterpret_cast<const uint32_t*>(buf.get()));
                return input.read_exactly(sz);
            }).then([this, &output] (temporary_buffer<char> buf) mutable {
                if (!buf) {
                    return make_ready_future<stop_iteration>(stop_iteration::yes); // EOF
                }
                result res = result::deserialize(std::move(buf));
                if (res.found) {
                    // TODO: stop server entirely
                    return output.close().then([this] () mutable {
                        return close();
                    }).then([] {
                        return make_ready_future<stop_iteration>(stop_iteration::yes);
                    });
                }
                return send_next_task(output)
                .then([] {
                    return make_ready_future<stop_iteration>(stop_iteration::no);
                });
            });
        });
    }

    future<>
    send_next_task(output_stream<char> &output) {
        return smp::submit_to(0, [this, &output] () mutable {
            return tsk_gen->get_next();
        }).then([this, &output] (boost::optional<task> ot) {
            if (ot) {
                std::cerr << "Sending task with password=\"" << ot->password << "\"" << std::endl;
                ot->from = ot->to;
                return output.write(ot->serialize())
                .then([&output] () mutable {
                    return output.flush();
                });
            } else {
                std::cerr << "Closing connection" << std::endl;
                return output.close();  /* we should handle this better, mb raise an exception */
            }
        });
    }
    
public:
    app() {
        config::register_options(add_options());
    }
    int
    run(int ac, char ** av) {
        return app_template::run(ac, av, [this] {
            auto config = seabrute::config(configuration());
            tsk_gen = std::make_shared<seabrute::task_generator>(seabrute::task(config.alph, 0, config.length, config.hash, std::string(config.length, '\0')));
            return parallel_for_each(boost::irange<unsigned int>(0, smp::count), [this] (unsigned int core) mutable {
                return smp::submit_to(core, [this] () mutable {return main_async();});
            });
        });
    }
};

} /* namespace seabrute */

int main(int argc, char** argv) {
    seabrute::app app;
    try {
        return app.run(argc, argv);
    } catch(std::runtime_error &e) {
        std::cerr << "Couldn't start application: " << e.what() << std::endl;
        return 1;
    }
}
