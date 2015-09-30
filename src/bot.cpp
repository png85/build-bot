#include <build-bot/bot.h>

#include <sys/types.h>
#include <sys/stat.h>

#include <chrono>
#include <thread>

#include <boost/asio.hpp>
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

                m_configFile = config_file;

                return true;
            }

            bool initFifo()
            {
                std::string fifoName;
                try {
                    fifoName = m_settings.get<std::string>("fifo.name");
                }

                catch (boost::property_tree::ptree_error& ex) {
                    BOOST_LOG_SEV(log, severity::error) << "Failed to get FIFO settings from configuration: " << ex.what();
                    return false;
                }

                fs::path path(fifoName);
                if (!fs::exists(path)) {
                    BOOST_LOG_SEV(log, severity::warning) << "FIFO " << fifoName << " doesn't exist; trying to create it!";
                    if (mkfifo(fifoName.c_str(), 0666) == -1) {
                        BOOST_LOG_SEV(log, severity::error) << "Failed to create FIFO " << fifoName << ": " << strerror(errno);
                        return false;
                    }
                }

                return true;
            }

            boost::asio::io_service m_io;
            boost::asio::strand m_strand;

            std::atomic<bool> m_stopRequested;
            std::atomic<bool> m_restartAfterStop;

            std::string m_configFile;

        public:
            Bot()
                : m_io()
                , m_strand(m_io)
                , m_stopRequested(false)
                , m_restartAfterStop(false)
                , m_configFile("")
            {
            }

            bool init(const std::string& config_file)
            {
                if (!loadConfig(config_file))
                    return false;

                if (!initFifo())
                    return false;

                return true;
            }

            void stop(bool restart = false)
            {
                if (restart)
                    m_restartAfterStop = true;
                m_stopRequested = true;
            }

            dsn::build_bot::Bot::ExitCode run()
            {
                BOOST_LOG_SEV(log, severity::trace) << "Starting io_service";
                std::thread ioServiceThread([&]() {
		    m_io.run();
                });

                while (!m_stopRequested.load()) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }

                BOOST_LOG_SEV(log, severity::trace) << "Stopping io_service";
                m_io.stop();
                ioServiceThread.join();

                if (m_restartAfterStop.load())
                    return dsn::build_bot::Bot::ExitCode::Restart;

                return dsn::build_bot::Bot::ExitCode::Success;
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

void Bot::stop()
{
    return m_impl->stop();
}

void Bot::restart()
{
    return m_impl->stop(true);
}

Bot::ExitCode Bot::run()
{
    return m_impl->run();
}
