#include "core/app-template.hh"
#include "core/seastar.hh"
#include "core/reactor.hh"
#include "core/future-util.hh"
#include <iostream>

using unconsumed_remainder = std::experimental::optional<temporary_buffer<char>>;

struct Consumer {
    int * counter;
    output_stream<char> *output;

    Consumer(int * _counter, output_stream<char> *_output) : counter(_counter), output(_output) {
    }

    future<unconsumed_remainder> operator()(temporary_buffer<char> buf) {
        if (buf) {
            std::copy(buf.begin(), buf.end(), std::ostream_iterator<char>(std::cout));
            std::cout.flush();
            return smp::submit_to(0, [&] {
                return ++(*counter);
            }).then([&] (int value) {
                std::stringstream s;
                s << "We received this value of the counter " << value << std::endl;
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
handle_connection (int * counter, connected_socket s, socket_address a) {
    input_stream<char> input = s.input();
    output_stream<char> output = s.output();
    return do_with(std::move(input), std::move(output), [counter] (input_stream<char> &input, output_stream<char> &output) {
        auto consumer = Consumer(counter, &output);
        return do_with(std::move(consumer), std::move(input), [] (auto &my_consumer, auto &input) {
            return input.consume(my_consumer);
        });
    });
}

future<>
main_async(int * counter) {
    listen_options lo;
    lo.reuse_address = true;
    return do_with(listen(make_ipv4_address({1234}), lo), [counter] (auto& listener) {
        return keep_doing([&listener, counter] () {
            return listener.accept().then([counter] (connected_socket s, socket_address a) {
                handle_connection(counter, std::move(s), std::move(a));
            });
        });
    });
}

int main(int argc, char** argv) {
    int counter = 0;
    app_template app;
    try {
        app.run(argc, argv, [&counter] {
            return parallel_for_each(boost::irange<unsigned int>(0, smp::count), [&counter] (unsigned int core) {
                return smp::submit_to(core, [&counter] {return main_async(&counter);});
            });
        });
    } catch(std::runtime_error &e) {
        std::cerr << "Couldn't start application: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
