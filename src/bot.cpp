#include <build-bot/bot.h>

#include <boost/filesystem.hpp>

namespace fs = boost::filesystem;
using namespace dsn::build_bot;

namespace dsn {
namespace build_bot {
    namespace priv {
        class Bot : public dsn::log::Base<Bot> {
        protected:
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
