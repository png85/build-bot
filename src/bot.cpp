#include <build-bot/bot.h>

using namespace dsn::build_bot;

namespace dsn {
namespace build_bot {
    namespace priv {
        class Bot : public dsn::log::Base<Bot> {
        public:
            bool init(const std::string& config_file)
            {
                return true;
            }
        };
    }
}
}

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
