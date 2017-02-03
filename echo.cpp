#include "core/app-template.hh"
#include "core/seastar.hh"
#include "core/reactor.hh"
#include "core/future-util.hh"
#include <iostream>

using unconsumed_remainder = std::experimental::optional<temporary_buffer<char>>;

struct Consumer {
    output_stream<char> *output;
    Consumer(output_stream<char> *_output) :output(_output) {
    }

    future<unconsumed_remainder> operator()(temporary_buffer<char> buf) {
        if (buf) {
            std::copy(buf.begin(), buf.end(), std::ostream_iterator<char>(std::cout));
            std::cout.flush();
            return output->write(std::move(buf)).then([&] () { 
                return output->flush();
            }).then([] () {
                return unconsumed_remainder();
            });
        } else {
            return make_ready_future<unconsumed_remainder>(unconsumed_remainder());
        }
    }
};

future<>
handle_connection (connected_socket s, socket_address a) {
    input_stream<char> input = s.input();
    output_stream<char> output = s.output();
    return do_with(std::move(s), std::move(input), std::move(output), [] (connected_socket &ss, input_stream<char> &input, output_stream<char> &output) {
        auto consumer = Consumer(&output);
        return do_with(std::move(consumer), std::move(input), [] (auto &my_consumer, auto &input) {
            return input.consume(my_consumer);
                });
    });
}

future<>
main_async() {
    return do_with(listen(make_ipv4_address({1234})), [] (auto& listener) {
        return keep_doing([&listener] () {
            return listener.accept().then([] (connected_socket s, socket_address a) {
                handle_connection(std::move(s), std::move(a));
            });
        });
    });
}

int main(int argc, char** argv) {
    app_template app;
    try {
        app.run(argc, argv, main_async);
    } catch(std::runtime_error &e) {
        std::cerr << "Couldn't start application: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
