#ifndef NSTORE_STATS_H
#define NSTORE_STATS_H

#include <chrono>
#include <cstddef>

namespace stats {

extern time_t startup;
extern size_t no_connects;
extern size_t no_disconnects;
extern size_t no_ro_txns;
extern size_t no_w_txns;

} // end of stats namespace

#endif // NSTORE_STATS_H

