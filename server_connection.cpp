#include <boost/range/irange.hpp>
#include <util/log.hh>
#include "result.hpp"
#include "server_connection.hpp"

static seastar::logger logger("server_connection");

namespace seabrute {

using seastar::do_for_each;
using seastar::make_ready_future;
using seastar::stop_iteration;
using seastar::temporary_buffer;

future<> server_connection::read_cycle(app *_app) {
    return seastar::repeat([this, _app] () mutable {
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
                logger.debug("Found result on connection {}", this);
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

future<> server_connection::send_next_task(app *_app) {
    return _app->get_next_task().then([this] (task ot) mutable {
        logger.debug("Sending task with password=\"{}\"", ot.password);
        ot.from = ot.to;
        return output.write(ot.serialize())
        .then([this] () mutable {
            return output.flush();
        });
    });
}

server_connection::server_connection(connected_socket &&_cs): cs(std::move(_cs)), input(cs.input()), output(cs.output()) {} 

future<> server_connection::life_cycle(app *_app) {
    /* send all 4 tasks, then in consumer parse one result and send one task */
    auto range = boost::irange(0, 4);
    return do_for_each(range, [this, _app] (int) mutable {
        return send_next_task(_app);
    }).then([this, _app] () mutable {
        return read_cycle(_app);
    }).handle_exception_type([this] (task_generator::generator_finished&) mutable {
        logger.debug("Closing connection");
        return output.close();  /* we should handle this better, mb raise an exception */
    });
}

}
