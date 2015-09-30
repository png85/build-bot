#include <build-bot/bot.h>

#include <boost/filesystem.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>

namespace fs = boost::filesystem;
using namespace dsn::build_bot;

namespace dsn {
namespace build_bot {
    namespace priv {
        class Bot : public dsn::log::Base<Bot> {
        protected:
            boost::property_tree::ptree m_settings;

            bool loadConfig(const std::string& config_file)
            {
                BOOST_LOG_SEV(log, severity::info) << "Loading configuration from " << config_file;

                fs::path path(config_file);
                if (!fs::exists(path)) {
                    BOOST_LOG_SEV(log, severity::error) << "Config file " << config_file << " doesn't exist!";
                    return false;
                }

                if (!fs::is_regular_file(path)) {
                    BOOST_LOG_SEV(log, severity::error) << "Config file " << config_file << " isn't a regular file!";
                    return false;
                }

                try {
                    boost::property_tree::read_ini(config_file, m_settings);
                }

                catch (boost::property_tree::ini_parser_error& ex) {
                    BOOST_LOG_SEV(log, severity::error) << "Failed to parse config file " << config_file << ": " << ex.what();
                    return false;
                }

                return true;
            }

        public:
            bool init(const std::string& config_file)
            {
                if (!loadConfig(config_file))
                    return false;

                return true;
            }
        };
    }
}
}

const std::string dsn::build_bot::Bot::DEFAULT_CONFIG_FILE{ "etc/build-bot/bot.conf" };

Bot::Bot()
    : m_impl(new priv::Bot())
{
}

Bot::~Bot()
{
}

bool Bot::init(const std::string& config_file)
{
    return m_impl->init(config_file);
}
