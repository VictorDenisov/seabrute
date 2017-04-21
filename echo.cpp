#include "core/app-template.hh"
#include "core/seastar.hh"
#include "core/reactor.hh"
#include "core/future-util.hh"
#include <iostream>
#include <vector>
#include <string>

namespace seabrute {

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
    std::string alph;
    int from, to;
    std::string hash;
    std::string password;
    task(std::string alph, int from, int to, std::string hash, std::string password) noexcept :
        alph(alph), from(from), to(to), hash(hash), password(password) {}
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
        current.password.replace(current.from, len, len, current.alph[0]);
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
            if (*i == current.alph.length() - 1) {
                *i = 0;
                *pos = current.alph[0];
            } else {
                ++*i;
                *pos = current.alph[*i];
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
            std::copy(buf.begin(), buf.end(), std::ostream_iterator<char>(std::cout));
            std::cout.flush();
            return smp::submit_to(0, [&] {
                return tsk_gen->get_next();
            }).then([&] (boost::optional<task> ot) {
                std::stringstream s;
                if (ot) {
                    s << "We received this value of the counter " << ot->password << std::endl;
                } else {
                    s << "We didn't receive any value." << std::endl;
                }
                output->write(s.str()).then([this] () {
                    return output->flush();
                });
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
        auto c = consumer(tsk_gen, &output);
        return do_with(std::move(c), std::move(input), [] (auto &c, auto &input) {
            return input.consume(c);
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
