#include <build-bot/worker.h>

#include <boost/filesystem.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include <boost/random/random_device.hpp>
#include <boost/random/uniform_int_distribution.hpp>

#include <boost/process.hpp>

namespace fs = boost::filesystem;

namespace dsn {
namespace build_bot {
    namespace priv {
        class Worker : public dsn::log::Base<Worker> {
        private:
            std::string m_url;
            std::string m_branch;
            std::string m_revision;
            std::string m_configFile;
            std::string m_profileName;
            std::string m_buildDir;
            std::string m_repoName;
            std::string m_macroFile;

            std::string generateBuildId()
            {
                std::string res;
                boost::random::random_device rng;
                boost::random::uniform_int_distribution<> dist(0, BUILD_ID_CHARS.size() - 1);

                for (size_t i = 0; i < BUILD_ID_LENGTH; i++)
                    res += BUILD_ID_CHARS[dist(rng)];

                return res;
            }

            std::string m_buildId;

            static const std::string BUILD_ID_CHARS;
            static const size_t BUILD_ID_LENGTH;

            std::string m_toplevelDirectory;

            bool initToplevelDirectory()
            {
                std::stringstream ss;
                ss << m_buildDir << "/" << m_profileName << "/" << m_repoName << "/" << m_buildId;
                m_toplevelDirectory = ss.str();
                fs::path path(m_toplevelDirectory);

                BOOST_LOG_SEV(log, severity::info) << "Toplevel build directory is " << m_toplevelDirectory;

                if (!fs::exists(path)) {
                    try {
                        fs::create_directories(path);
                    }

                    catch (boost::system::system_error& ex) {
                        BOOST_LOG_SEV(log, severity::error) << "Failed to create toplevel directory " << m_toplevelDirectory << ": " << ex.what();
                        return false;
                    }
                }

                return true;
            }

            std::string m_gitExecutable;
            bool findGitExecutable()
            {
                try {
                    m_gitExecutable = boost::process::search_path("git");
                }
                catch (std::runtime_error& ex) {
                    BOOST_LOG_SEV(log, severity::error) << "Failed to locate git executable in PATH: " << ex.what();
                    return false;
                }

                if (m_gitExecutable.size() == 0) {
                    BOOST_LOG_SEV(log, severity::error) << "No git executable found in PATH!";
                    return false;
                }

                BOOST_LOG_SEV(log, severity::debug) << "Using git executable from " << m_gitExecutable;
                return true;
            }

            std::string m_sourceDirectory;
            bool checkoutSources()
            {
                m_sourceDirectory = m_toplevelDirectory + "/repo";
                BOOST_LOG_SEV(log, severity::info) << "Checking out sources from " << m_url << " to " << m_sourceDirectory;

                try {
                    std::stringstream ssArgs;
                    ssArgs << m_gitExecutable << " clone --recursive -b " << m_branch << " " << m_url << " " << m_sourceDirectory;
                    std::string args = ssArgs.str();
                    BOOST_LOG_SEV(log, severity::debug) << "Git command line is " << args;

                    boost::process::child child = boost::process::execute(boost::process::initializers::run_exe(m_gitExecutable),
                                                                          boost::process::initializers::set_cmd_line(args));
                    auto exit_code = boost::process::wait_for_exit(child);

                    if (exit_code != 0) {
                        BOOST_LOG_SEV(log, severity::error) << "Got non-zero exit status from '" << args << "': " << exit_code;
                        return false;
                    }
                }

                catch (boost::system::system_error& ex) {
                    BOOST_LOG_SEV(log, severity::error) << "Failed to run git to check out sources: " << ex.what();
                    return false;
                }

                BOOST_LOG_SEV(log, severity::info) << "Repository cloned successfully. Checking out revision " << m_revision;

                try {
                    std::stringstream ssArgs;
                    ssArgs << m_gitExecutable << " checkout " << m_revision << " .";
                    std::string args = ssArgs.str();
                    BOOST_LOG_SEV(log, severity::debug) << "Git command line is " << args;
                    boost::process::child child = boost::process::execute(boost::process::initializers::run_exe(m_gitExecutable),
                                                                          boost::process::initializers::set_cmd_line(args),
                                                                          boost::process::initializers::start_in_dir(m_sourceDirectory));
                    auto exit_code = boost::process::wait_for_exit(child);
                    if (exit_code != 0) {
                        BOOST_LOG_SEV(log, severity::error) << "Got non-zero exit status from '" << args << "': " << exit_code;
                        return false;
                    }
                }

                catch (boost::system::system_error& ex) {
                    BOOST_LOG_SEV(log, severity::error) << "Failed to run git to check out revision " << m_revision << ": " << ex.what();
                    return false;
                }

                BOOST_LOG_SEV(log, severity::info) << "Checkout out revision " << m_revision;

                return true;
            }

            boost::property_tree::ptree m_macros;
            bool loadMacroFile()
            {
                fs::path path(m_macroFile);
                if (!fs::exists(path)) {
                    BOOST_LOG_SEV(log, severity::warning) << "Macro file " << m_macroFile << " doesn't exist!";
                    return true;
                }

                if (!fs::is_regular_file(path)) {
                    BOOST_LOG_SEV(log, severity::error) << "Macro configuration " << m_macroFile << " exists but isn't a regular file!";
                    return false;
                }

                try {
                    boost::property_tree::read_ini(m_macroFile, m_macros);
                }

                catch (boost::property_tree::ptree_error& ex) {
                    BOOST_LOG_SEV(log, severity::error) << "Failed to parse macros from " << m_macroFile << ": " << ex.what();
                    return false;
                }

                return true;
            }

            boost::property_tree::ptree m_buildSettings;
            bool loadBuildConfig()
            {
                std::string configFile = m_sourceDirectory + "/" + m_configFile;
                fs::path path(configFile);
                if (!fs::exists(path)) {
                    BOOST_LOG_SEV(log, severity::error) << "Build config file " << m_configFile << " doesn't exist in " << m_sourceDirectory;
                    return false;
                }

                if (!fs::is_regular_file(path)) {
                    BOOST_LOG_SEV(log, severity::error) << "Build config file " << configFile << " exists but isn't a regular file!";
                    return false;
                }

                try {
                    boost::property_tree::read_ini(configFile, m_buildSettings);
                }

                catch (boost::property_tree::ini_parser_error& ex) {
                    BOOST_LOG_SEV(log, severity::error) << "Failed to parse build configuration in " << m_configFile << ": " << ex.what();
                    return false;
                }

                return true;
            }

        public:
            Worker(const std::string& macro_file, const std::string& build_directory,
                   const std::string& repo_name,
                   const std::string& url, const std::string& branch, const std::string& revision,
                   const std::string& config_file, const std::string& profile_name)
                : m_buildDir(build_directory)
                , m_macroFile(macro_file)
                , m_url(url)
                , m_branch(branch)
                , m_revision(revision)
                , m_configFile(config_file)
                , m_profileName(profile_name)
                , m_repoName(repo_name)
            {
            }

            void run()
            {
                if (!findGitExecutable()) {
                    BOOST_LOG_SEV(log, severity::error) << "Failed to find git; build FAILED!";
                    return;
                }

                m_buildId = generateBuildId();
                BOOST_LOG_SEV(log, severity::info) << "Worker started for repo " << m_repoName
                                                   << " (profile: " << m_profileName << ", config: " << m_configFile << ") - Build ID: " << m_buildId;
                if (!initToplevelDirectory()) {
                    BOOST_LOG_SEV(log, severity::error) << "Unable to create build directory; build FAILED!";
                    return;
                }

                if (!loadMacroFile()) {
                    BOOST_LOG_SEV(log, severity::error) << "Failed to load macro file; build FAILED!";
                    return;
                }

                if (!checkoutSources()) {
                    BOOST_LOG_SEV(log, severity::error) << "Failed to checkout sources from " << m_url << "; build FAILED!";
                    return;
                }

                if (!loadBuildConfig()) {
                    BOOST_LOG_SEV(log, severity::error) << "Failed to load build configuration; build FAILED!";
                    return;
                }
            }
        };

        const std::string Worker::BUILD_ID_CHARS{ "0123456789abcdef" };
        const size_t Worker::BUILD_ID_LENGTH{ 8 };
    }
}
}

using namespace dsn::build_bot;

Worker::Worker(const std::string& macro_file, const std::string& build_directory,
               const std::string& repo_name,
               const std::string& url, const std::string& branch, const std::string& revision,
               const std::string& config_file, const std::string& profile_name)
    : m_impl(new priv::Worker(macro_file, build_directory, repo_name, url, branch, revision, config_file, profile_name))
{
}

Worker::~Worker()
{
}

void Worker::run()
{
    return m_impl->run();
}
