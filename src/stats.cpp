#include "stats.h"

namespace stats {

time_t startup;
size_t no_connects = 0;
size_t no_disconnects = 0;
size_t no_ro_txns = 0;
size_t no_w_txns = 0;

} // end of stats namespace

