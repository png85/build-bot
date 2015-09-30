#include <build-bot/worker.h>

namespace dsn {
namespace build_bot {
    namespace priv {
        class Worker : public dsn::log::Base<Worker> {
            std::string m_url;
            std::string m_branch;
            std::string m_revision;
            std::string m_configFile;
            std::string m_profileName;

        public:
            Worker(const std::string& url, const std::string& branch, const std::string& revision, const std::string& config_file, const std::string& profile_name)
                : m_url(url)
                , m_branch(branch)
                , m_revision(revision)
                , m_configFile(config_file)
                , m_profileName(profile_name)
            {
            }

            void run()
            {
                BOOST_LOG_SEV(log, severity::debug) << "Worker started for repo " << m_url << "/" << m_branch << " (" << m_revision << ")"
                                                    << ", profile=" << m_profileName << "; config=" << m_configFile;
            }
        };
    }
}
}

using namespace dsn::build_bot;

Worker::Worker(const std::string& url, const std::string& branch, const std::string& revision, const std::string& config_file, const std::string& profile_name)
    : m_impl(new priv::Worker(url, branch, revision, config_file, profile_name))
{
}

Worker::~Worker()
{
}

void Worker::run()
{
    return m_impl->run();
}
