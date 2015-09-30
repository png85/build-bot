#include <boost/log/trivial.hpp>
#include <boost/program_options.hpp>

#include <dsnutil/log/init.h>

namespace po = boost::program_options;

int main(int argc, char** argv)
{
    dsn::log::init();

    try {
        po::options_description descr;
        descr.add_options()("help,?", "Display list of valid arguments");

        po::variables_map vm;
        po::store(po::parse_command_line(argc, argv, descr), vm);
        po::notify(vm);
    }

    catch (po::error& ex) {
        BOOST_LOG_TRIVIAL(error) << "Failed to parse command line arguments: " << ex.what();
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
