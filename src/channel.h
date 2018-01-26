#ifndef NSTORE_CHANNEL_H
#define NSTORE_CHANNEL_H

#include <cstdint>
#include <map>
#include <memory>
#include <set>

class session;

namespace channel {

void join(uint32_t ns, std::shared_ptr<session> member);
void leave(uint32_t ns, std::shared_ptr<session> member);
void publish(uint32_t ns, std::shared_ptr<session> from, const std::string& msg);
void clear();

}

#endif // NSTORE_CHANNEL_H
