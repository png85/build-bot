#include <iostream>
#include <unistd.h>

#include <boost/log/trivial.hpp>
#include <boost/program_options.hpp>

#include <dsnutil/log/init.h>
#include <build-bot/bot.h>

namespace po = boost::program_options;

int main(int argc, char** argv)
{
    dsn::log::init();

    std::string configFile;
    try {
        po::options_description descr;
        descr.add_options()("help,?", "Display list of valid arguments");
        descr.add_options()("config,c", po::value<std::string>()->required()->default_value(dsn::build_bot::Bot::DEFAULT_CONFIG_FILE), "Path to configuration file");

        po::variables_map vm;
        po::store(po::parse_command_line(argc, argv, descr), vm);
        po::notify(vm);

        if (vm.count("help")) {
            std::cerr << descr << std::endl;
            return EXIT_SUCCESS;
        }

        configFile = vm["config"].as<std::string>();
    }

    catch (po::error& ex) {
        BOOST_LOG_TRIVIAL(error) << "Failed to parse command line arguments: " << ex.what();
        return EXIT_FAILURE;
    }

    dsn::build_bot::Bot& bot = dsn::build_bot::Bot::instanceRef();
    if (!bot.init(configFile)) {
        BOOST_LOG_TRIVIAL(error) << "Failed to initialize; exiting!";
        return EXIT_FAILURE;
    }

    switch (bot.run()) {
    case dsn::build_bot::Bot::ExitCode::Success:
        return EXIT_SUCCESS;
    case dsn::build_bot::Bot::ExitCode::Failure:
        return EXIT_FAILURE;
    case dsn::build_bot::Bot::ExitCode::Restart:
        if (execv(argv[0], argv) == -1) {
            BOOST_LOG_TRIVIAL(fatal) << "execv() failed: " << strerror(errno);
            return EXIT_FAILURE;
        }
    }

    BOOST_LOG_TRIVIAL(warning) << "Invalid exit code from Bot::run()!";
    return EXIT_SUCCESS;
}
