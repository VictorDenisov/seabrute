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
    static const int default_length = 4;
};

const std::string config::default_alph = std::string("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789");
const std::string config::default_hash = std::string("eMWCfUJDk9Lec");

/********************/

struct task {
    std::string alph;
    int from, to;
    std::string hash;
    std::string password;
    task(std::string alph, int from, int to, std::string hash, std::string password) :
        alph(alph), from(from), to(to), hash(hash), password(password) {}
};

class task_generator {
    task current;
public:
    task_generator(task &&task) : current(task) {}
    task get_next() {
        return current;
    }
};

using unconsumed_remainder = std::experimental::optional<temporary_buffer<char>>;

struct consumer {
    task_generator *tsk_gen;
    output_stream<char> *output;

    consumer(task_generator *_tsk_gen, output_stream<char> *_output) : tsk_gen(_tsk_gen), output(_output) {
    }

    future<unconsumed_remainder> operator()(temporary_buffer<char> buf) {
        if (buf) {
            std::copy(buf.begin(), buf.end(), std::ostream_iterator<char>(std::cout));
            std::cout.flush();
            return smp::submit_to(0, [&] {
                return tsk_gen->get_next();
            }).then([&] (task t) {
                std::stringstream s;
                s << "We received this value of the counter " << std::endl;
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
handle_connection (task_generator *tsk_gen, connected_socket s, socket_address a) {
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
main_async(task_generator *tsk_gen) {
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
            seabrute::task_generator tsk_gen(seabrute::task(config.alph, 0, config.length, config.hash, std::string(config.length, '\0')));
            return parallel_for_each(boost::irange<unsigned int>(0, smp::count), [&tsk_gen] (unsigned int core) {
                return smp::submit_to(core, [&tsk_gen] {return main_async(&tsk_gen);});
            });
        });
    } catch(std::runtime_error &e) {
        std::cerr << "Couldn't start application: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
