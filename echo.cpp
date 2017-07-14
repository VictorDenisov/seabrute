#include "core/app-template.hh"
#include "core/seastar.hh"
#include "core/reactor.hh"
#include "core/future-util.hh"
#include <boost/range/irange.hpp>
#include <iostream>
#include <list>
#include <vector>
#include <string>
#include <arpa/inet.h>
#include "config.hpp"

namespace seabrute {

#include "legacy.hpp"

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
    class generator_finished : std::exception {
        virtual const char* what() const noexcept {
            return "Generator finished";
        }
    };
    task_generator(task &&task) : current(task), index(task.to - task.from), started(false), finished(false) {
        int len = current.to - current.from;
        if (len == 0)
            finished = true;
        current.password.replace(current.from, len, len, (*current.alph)[0]);
        std::cerr << "Task generator address " << (void*)this << std::endl;
        original_address = this;
    }
    task get_next() {
        std::cerr << "Task generator address in get_next " << (void*)this << std::endl;
        assert (this == original_address);
        if (finished)
            throw generator_finished();
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
        throw generator_finished();
    }
};
task_generator* task_generator::original_address = nullptr;

template<class T>
class self_deleting_weak_ref {
    using container_t = std::list<self_deleting_weak_ref<T>*>;
    container_t *cont;
    std::weak_ptr<T> weak_ref;
    typename container_t::iterator pos;

    self_deleting_weak_ref(container_t &cont) : cont(&cont), pos(cont.insert(cont.begin(), this)) {
    }

    class deleter {
        self_deleting_weak_ref<T> *sdwr_ptr;
    public:
        deleter(self_deleting_weak_ref<T> *p) : sdwr_ptr(p) {}
        void operator()(T *p) const {
            sdwr_ptr->cont->erase(sdwr_ptr->pos);
            delete sdwr_ptr;
            delete p;
        }
    };

public:
    template<class U>
    static std::shared_ptr<T> create(container_t &cont, U &&arg) {
        T *p = new T(std::move(arg));
        auto sdwr = new self_deleting_weak_ref(cont);
        deleter a_deleter(sdwr);
        auto sp = std::shared_ptr<T>(p, a_deleter);
        sdwr->weak_ref = sp;
        return sp;
    }
    std::shared_ptr<T> lock() {
        return weak_ref.lock();
    }
};

class app;

class listener : public std::enable_shared_from_this<listener> {
    server_socket ss;
public:
    listener(server_socket &&_ss) : ss(std::move(_ss)) {}
    future<> accept_loop(app *_app);
    void close() {
        std::cerr << "Closing listener " << this << std::endl;
        ss.abort_accept();
    }
};

class app : public app_template {
    std::shared_ptr<seabrute::task_generator> tsk_gen;
    typename self_deleting_weak_ref<listener>::container_t listeners;
    bool closing = false;

    future<>
    main_async(unsigned int core) {
        listen_options lo;
        lo.reuse_address = true;
        std::cerr << "Listen on core " << core << std::endl;
        auto list = listen(make_ipv4_address({1234}), lo);
        std::cerr << "Creating socket on core " << core << std::endl;
        auto ss = server_socket(std::move(list));
        if (closing) {
            std::cerr << "App is already closing, not creating socket on core " << core << std::endl;
            return make_ready_future<>();
        }
        return add_listener(std::move(ss))
        .then([this] (std::shared_ptr<listener> l) mutable {
            return l->accept_loop(this);
        });
    }

    future<std::shared_ptr<listener>>
    add_listener(server_socket &&ss) {
        std::cerr << "Adding listener.." << std::endl;
        return smp::submit_to(0, [this, ss = std::move(ss)] () mutable {
            auto ptr = self_deleting_weak_ref<listener>::create(listeners, ss);
            std::cerr << "Added listener " << ptr << std::endl;
            return make_ready_future<std::shared_ptr<listener>>(ptr);
        });
    }

public:
    app() {
        config::register_options(add_options());
    }

    bool
    is_closing() {
        return closing;
    }

    future<task>
    get_next_task() {
        return smp::submit_to(0, [this] () mutable {
            return tsk_gen->get_next();
        });
    }

    future<>
    close() {
        std::cerr << "Closing app " << this << std::endl;
        closing = true;
        return smp::submit_to(0, [this] () mutable {
            std::cerr << "Closing app " << this << " on CPU #0" << std::endl;
            for (auto weak_listener : listeners) {
                std::cerr << "Going to close listener by ref " << weak_listener << std::endl;
                auto listener = weak_listener->lock();
                if (listener) {
                    listener->close();
                }
            }
            return make_ready_future<>();
        });
    }
    
    int
    run(int ac, char ** av) {
        return app_template::run(ac, av, [this] {
            auto config = seabrute::config(configuration());
            tsk_gen = std::make_shared<seabrute::task_generator>(seabrute::task(config.alph, 0, config.length, config.hash, std::string(config.length, '\0')));
            std::cerr << "Will start " << smp::count << " listeners" << std::endl;
            return parallel_for_each(boost::irange<unsigned int>(0, smp::count), [this] (unsigned int core) mutable {
                std::cerr << "Starting listener on core " << core << std::endl;
                return smp::submit_to(core, [this, core] () mutable {
                    std::cerr << "Starting main_async on core " << core << std::endl;
                    return main_async(core);
                });
            });
        });
    }
};

class server_connection {
    connected_socket cs;
    input_stream<char> input;
    output_stream<char> output;

    future<>
    read_cycle(app *_app) {
        return repeat([this, _app] () mutable {
            return input.read_exactly(sizeof(uint32_t))
            .then([this] (temporary_buffer<char> buf) {
                if (!buf) {
                    return make_ready_future<temporary_buffer<char>>(std::move(buf));
                }
                uint32_t sz = ntohl(*reinterpret_cast<const uint32_t*>(buf.get()));
                return input.read_exactly(sz);
            }).then([this, _app] (temporary_buffer<char> buf) mutable {
                if (!buf) {
                    return make_ready_future<stop_iteration>(stop_iteration::yes); // EOF
                }
                result res = result::deserialize(std::move(buf));
                if (res.found) {
                    std::cerr << "Found result on connection " << this << std::endl;
                    // TODO: stop server entirely
                    return output.close().then([_app] () mutable {
                        return _app->close();
                    }).then([] {
                        return make_ready_future<stop_iteration>(stop_iteration::yes);
                    });
                }
                return send_next_task(_app).then([] {
                    return make_ready_future<stop_iteration>(stop_iteration::no);
                });
            });
        });
    }

    future<>
    send_next_task(app *_app) {
        return _app->get_next_task().then([this] (task ot) mutable {
            std::cerr << "Sending task with password=\"" << ot.password << "\"" << std::endl;
            ot.from = ot.to;
            return output.write(ot.serialize())
            .then([this] () mutable {
                return output.flush();
            });
        });
    }

public:
    server_connection(connected_socket &&_cs): cs(std::move(_cs)), input(cs.input()), output(cs.output()) {} 

    future<>
    life_cycle(app *_app) {
        /* send all 4 tasks, then in consumer parse one result and send one task */
        auto range = boost::irange(0, 4);
        return do_for_each(range, [this, _app] (int) mutable {
            return send_next_task(_app);
        }).then([this, _app] () mutable {
            return read_cycle(_app);
        }).handle_exception_type([this] (task_generator::generator_finished&) mutable {
            std::cerr << "Closing connection" << std::endl;
            return output.close();  /* we should handle this better, mb raise an exception */
        });
    }
};

future<>
listener::accept_loop(app *_app) {
    auto sthis = shared_from_this();
    return repeat([sthis, _app] () mutable {
        std::cerr << "Starting accept loop for listener " << sthis << std::endl;
        if (_app->is_closing()) {
            std::cerr << "App is closing, get out of here " << sthis << std::endl;
            return make_ready_future<stop_iteration>(stop_iteration::yes);
        }
        return sthis->ss.accept().then([sthis, _app] (connected_socket s, socket_address a) mutable {
            std::cerr << "Accepted connection from " << a << " in listener " << sthis << std::endl;
            return do_with(server_connection(std::move(s)), [_app] (server_connection &sc) {
                return sc.life_cycle(_app);
            });
        }).then([] {
            return make_ready_future<stop_iteration>(stop_iteration::no);
        });
    });
}


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
