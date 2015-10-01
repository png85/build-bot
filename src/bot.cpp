#include <build-bot/bot.h>
#include <build-bot/worker.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <chrono>
#include <thread>

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/filesystem.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include <boost/regex.hpp>

#include <dsnutil/threadpool.h>

namespace fs = boost::filesystem;
using namespace dsn::build_bot;

namespace dsn {
namespace build_bot {
    namespace priv {
        class Bot : public dsn::log::Base<Bot> {
        protected:
            boost::property_tree::ptree m_settings;
            boost::property_tree::ptree m_repositories;

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

                BOOST_LOG_SEV(log, severity::debug) << "Opening FIFO " << fifoName << " for reading.";
                int fd{ -1 };
                if ((fd = open(fifoName.c_str(), O_RDWR)) == -1) {
                    BOOST_LOG_SEV(log, severity::error) << "Failed to open FIFO " << fifoName << " for reading: " << strerror(errno);
                    return false;
                }

                boost::system::error_code error = m_fifo.assign(fd, error);
                if (error) {
                    BOOST_LOG_SEV(log, severity::error) << "Failed to assign FIFO fd to stream_descriptor: " << boost::system::system_error(error).what();
                    close(fd);
                    return false;
                }

                return true;
            }

            bool initRepositories()
            {
                std::string repoFile;
                try {
                    repoFile = m_settings.get<std::string>("repositories.config", DEFAULT_REPO_CONFIG);
                }

                catch (boost::property_tree::ptree_error& ex) {
                    BOOST_LOG_SEV(log, severity::error) << "Unable to get repository config file from settings: " << ex.what();
                    return false;
                }

                BOOST_LOG_SEV(log, severity::info) << "Initializing repository configuration from " << repoFile;
                fs::path path(repoFile);
                if (!fs::exists(path)) {
                    BOOST_LOG_SEV(log, severity::error) << "Repository config file " << repoFile << " doesn't exist!";
                    return false;
                }

                if (!fs::is_regular_file(path)) {
                    BOOST_LOG_SEV(log, severity::error) << "Repository config " << repoFile << " isn't a regular file!";
                    return false;
                }

                try {
                    boost::property_tree::read_ini(repoFile, m_repositories);
                }

                catch (boost::property_tree::ini_parser_error& ex) {
                    BOOST_LOG_SEV(log, severity::error) << "Failed to parse repository configuration from " << repoFile << ": " << ex.what();
                    return false;
                }

                return true;
            }

            std::string m_buildDirectory;

            bool initBuildDirectory()
            {
                std::string buildDir;
                try {
                    buildDir = m_settings.get<std::string>("fs.build_dir");
                }

                catch (boost::property_tree::ptree_error& ex) {
                    BOOST_LOG_SEV(log, severity::error) << "Unable to get build directory from configuration: " << ex.what();
                    return false;
                }

                fs::path path(buildDir);
                if (!fs::exists(path)) {
                    BOOST_LOG_SEV(log, severity::error) << "Build directory " << buildDir << "doesn't exist!";
                    return false;
                }

                if (!fs::is_directory(path)) {
                    BOOST_LOG_SEV(log, severity::error) << "Configured build path " << buildDir << " isn't a directory!";
                    return false;
                }

                BOOST_LOG_SEV(log, severity::info) << "Build directory is " << buildDir;
                m_buildDirectory = buildDir;

                return true;
            }

            boost::asio::io_service m_io;
            boost::asio::strand m_strand;

            std::atomic<bool> m_stopRequested;
            std::atomic<bool> m_restartAfterStop;

            std::string m_configFile;

            boost::asio::posix::stream_descriptor m_fifo;
            boost::asio::streambuf m_buffer;

            void read(const boost::system::error_code& error)
            {
                if (error) {
                    BOOST_LOG_SEV(log, severity::error) << "Failed to read from FIFO: " << boost::system::system_error(error).what();
                    return;
                }

                std::string message;
                {
                    std::istream stream(&m_buffer);
                    std::getline(stream, message);
                }

                if (!parse(message))
                    BOOST_LOG_SEV(log, severity::warning) << "Failed to parse message from FIFO: " << message;

                boost::asio::async_read_until(m_fifo, m_buffer, "\n", boost::bind(&Bot::read, this, boost::asio::placeholders::error));
            }

            bool parse(const std::string& message)
            {
                if (message == "STOP") {
                    BOOST_LOG_SEV(log, severity::info) << "Got STOP command on FIFO!";
                    stop();
                    return true;
                }

                if (message == "RESTART") {
                    BOOST_LOG_SEV(log, severity::info) << "Got RESTART command on FIFO!";
                    stop(true);
                    return true;
                }

                try {
                    boost::regex BUILD_regex("^BUILD (.+) (.+) (.+) (.+)$");
                    boost::cmatch match;
                    if (boost::regex_match(message.c_str(), match, BUILD_regex)) {
                        std::string repoName(match[1].first, match[1].second);
                        std::string profileName(match[2].first, match[2].second);
                        std::string branchName(match[3].first, match[3].second);
                        std::string gitRevision(match[4].first, match[4].second);

                        BOOST_LOG_SEV(log, severity::info) << "Got BUILD request for repo=" << repoName << ", profile=" << profileName << ", SHA1: " << gitRevision;
                        std::string repoUrl;
                        std::string repoConfigFile;
                        try {
                            repoUrl = m_repositories.get<std::string>(repoName + ".url");
                            repoConfigFile = m_repositories.get<std::string>(repoName + ".config");
                        }
                        catch (boost::property_tree::ptree_error& ex) {
                            BOOST_LOG_SEV(log, severity::error) << "Unable to get configuration for repository " << repoName << ": " << ex.what();
                            return true;
                        }

                        m_threadPool.enqueue([=]() {
			    dsn::build_bot::Worker worker(repoUrl, branchName, gitRevision, repoConfigFile, profileName);
			    worker.run();
                        });

                        return true;
                    }
                }

                catch (boost::regex_error& ex) {
                    BOOST_LOG_SEV(log, severity::fatal) << "BUILD regex is malformed: " << ex.what();
                    return false;
                }

                return false;
            }

            dsn::ThreadPool m_threadPool;

        public:
            Bot()
                : m_io()
                , m_strand(m_io)
                , m_stopRequested(false)
                , m_restartAfterStop(false)
                , m_configFile("")
                , m_fifo(m_io)
            {
            }

            ~Bot()
            {
                if (m_fifo.is_open())
                    m_fifo.close();
            }

            bool init(const std::string& config_file)
            {
                if (!loadConfig(config_file))
                    return false;

                if (!initRepositories())
                    return false;

                if (!initBuildDirectory())
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
                BOOST_LOG_SEV(log, severity::trace) << "Installing async read handler for FIFO";
                boost::asio::async_read_until(m_fifo, m_buffer, "\n", boost::bind(&Bot::read, this, boost::asio::placeholders::error));

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

            static const std::string DEFAULT_REPO_CONFIG;
        };
    }
}
}

const std::string dsn::build_bot::Bot::DEFAULT_CONFIG_FILE{ "etc/build-bot/bot.conf" };
const std::string dsn::build_bot::priv::Bot::DEFAULT_REPO_CONFIG{ "etc/build-bot/repos.conf" };

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
