// -*- C++ -*-
#ifndef BUILD_BOT_WORKER_H
#define BUILD_BOT_WORKER_H 1

#include <memory>
#include <dsnutil/log/base.h>

namespace dsn {
namespace build_bot {
    namespace priv {
        class Worker;
    }
    class Worker : public dsn::log::Base<Worker> {
    public:
        Worker(const std::string& url, const std::string& branch, const std::string& revision, const std::string& config_file, const std::string& profile_name);
        void run();

    private:
        std::unique_ptr<priv::Worker> m_impl;
    };
}
}

#endif
