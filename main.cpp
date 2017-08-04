#include <iostream>
#include <util/log.hh>
#include "app.hpp"

static seastar::logger logger("main");

int main(int argc, char** argv) {
    seabrute::app app;
    try {
        return app.run(argc, argv);
    } catch(std::runtime_error &e) {
        logger.debug("Couldn't start application: {}", e.what());
        return 1;
    }
}
