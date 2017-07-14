#ifndef __CONFIG_HPP__
#define __CONFIG_HPP__

#include <boost/program_options/options_description.hpp>
#include <boost/program_options/variables_map.hpp>

namespace bpo = boost::program_options;

namespace seabrute {

class config {
    static const std::string default_alph;
    static const std::string default_hash;
    static const int default_length;
public:
    std::string alph;
    std::string hash;
    int length;

    config(bpo::variables_map &args);
    static void register_options(bpo::options_description_easy_init &&add_options);
};

}

#endif // __CONFIG_HPP__
