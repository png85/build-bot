// -*- C++ -*-
#ifndef BUILD_BOT_BOT_H
#define BUILD_BOT_BOT_H 1

#include <memory>

#include <dsnutil/singleton.h>
#include <dsnutil/log/base.h>

namespace dsn {
namespace build_bot {
    namespace priv {
        class Bot;
    }
    class Bot
        : public dsn::Singleton<Bot>,
          public dsn::log::Base<Bot> {
        friend class dsn::Singleton<Bot>;

    protected:
        Bot();
        ~Bot();

        std::unique_ptr<priv::Bot> m_impl;

    public:
        bool init(const std::string& config_file);
        void stop();
        void restart();

        static const std::string DEFAULT_CONFIG_FILE;
    };
}
}

#endif // BUILD_BOT_BOT_H
