#include "config.hpp"

namespace seabrute {

//const std::string config::default_alph = std::string("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789");
const std::string config::default_alph = std::string("csit");
const std::string config::default_hash = std::string("eMWCfUJDk9Lec");
const int config::default_length = 4;

config::config(bpo::variables_map &args) :
    alph(args["alph"].as< std::string >()),
    hash(args["hash"].as< std::string >()),
    length(args["length"].as<int>()) {}

void config::register_options(bpo::options_description_easy_init &&add_options) {
    add_options
        ("alph,a", bpo::value< std::string >()->default_value(default_alph), "alphabet")
        ("hash,h", bpo::value< std::string >()->default_value(default_hash), "hash")
        ("length,n", bpo::value<int>()->default_value(default_length), "password length")
        ;
}

}
