#include "core/app-template.hh"
#include "core/seastar.hh"
#include "core/reactor.hh"
#include "core/future-util.hh"
#include <iostream>

namespace seabrute {

using unconsumed_remainder = std::experimental::optional<temporary_buffer<char>>;

struct task {
    int counter;
    task() : counter(0) {}
};

class task_generator {
    task current;
public:
    task get_next() {
        current.counter++;
        return current;
    }
};

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
                s << "We received this value of the counter " << t.counter << std::endl;
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
    seabrute::task_generator tsk_gen;
    app_template app;
    try {
        app.run(argc, argv, [&tsk_gen] {
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
