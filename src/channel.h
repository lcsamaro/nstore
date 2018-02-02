#ifndef NSTORE_CHANNEL_H
#define NSTORE_CHANNEL_H

#include <cstdint>
#include <memory>
#include <string>

#include "common.h"

class session;

namespace channel {

void join(u32 ns, std::shared_ptr<session> member);
void leave(u32 ns, std::shared_ptr<session> member);
void publish(u32 ns, std::shared_ptr<session> from, const std::string& msg);
void clear();

} // end of channel namespace

#endif // NSTORE_CHANNEL_H
