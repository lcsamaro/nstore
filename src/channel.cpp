#include "channel.h"
#include "session.h"

#include <map>
#include <set>

std::map<u32, std::set<std::shared_ptr<session>>> members;

namespace channel {

void join(u32 ns, std::shared_ptr<session> member) {
	if (members.find(ns) == members.end())
		members[ns] = std::set<std::shared_ptr<session>>();
	members[ns].insert(member);
}

void leave(u32 ns, std::shared_ptr<session> member) {
	if (members.find(ns) == members.end()) return;
	members[ns].erase(member);
}

void publish(u32 ns, std::shared_ptr<session> from, const std::string& msg) {
	if (members.find(ns) == members.end()) return;
	for (auto &m : members[ns]) if (m != from) m->write(msg);
}

void clear() {
	members.clear();
}

} // end of channel namespace

