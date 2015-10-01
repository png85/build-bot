#include <build-bot/worker.h>

#include <boost/filesystem.hpp>
#include <boost/random/random_device.hpp>
#include <boost/random/uniform_int_distribution.hpp>

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

            std::string m_sourceDirectory;
            bool checkoutSources()
            {
                m_sourceDirectory = m_toplevelDirectory + "/repo";
                BOOST_LOG_SEV(log, severity::info) << "Checking out sources from " << m_url << " to " << m_sourceDirectory;

                return true;
            }

        public:
            Worker(const std::string& build_directory,
                   const std::string& repo_name,
                   const std::string& url, const std::string& branch, const std::string& revision,
                   const std::string& config_file, const std::string& profile_name)
                : m_buildDir(build_directory)
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

                m_buildId = generateBuildId();
                BOOST_LOG_SEV(log, severity::info) << "Worker started for repo " << m_repoName
                                                   << " (profile: " << m_profileName << ", config: " << m_configFile << ") - Build ID: " << m_buildId;
                if (!initToplevelDirectory()) {
                    BOOST_LOG_SEV(log, severity::error) << "Unable to create build directory; build FAILED!";
                    return;
                }

                if (!checkoutSources()) {
                    BOOST_LOG_SEV(log, severity::error) << "Failed to checkout sources from " << m_url << "; build FAILED!";
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

Worker::Worker(const std::string& build_directory,
               const std::string& repo_name,
               const std::string& url, const std::string& branch, const std::string& revision,
               const std::string& config_file, const std::string& profile_name)
    : m_impl(new priv::Worker(build_directory, repo_name, url, branch, revision, config_file, profile_name))
{
}

Worker::~Worker()
{
}

void Worker::run()
{
    return m_impl->run();
}
