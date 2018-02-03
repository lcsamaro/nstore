#ifndef NSTORE_MOCKS_H
#define NSTORE_MOCKS_H

#include "common.h"
#include "session.h"

#include <cstdio>
#include <functional>
#include <memory>
#include <string>

#include <json.hpp>
using json = nlohmann::json;

#include <spdlog/spdlog.h>
extern std::shared_ptr<spdlog::logger> logger;

// service
enum { POOL_IO = 0, POOL_READ, POOL_WRITE };
class svc {
	int pool;
public:
	svc(int pool);
	void post(std::function<void()> f);
};

extern svc io_service;
extern svc reader_service;
extern svc writer_service;

struct session {
	std::string arg;
	session();
	void     write(const std::string& msg);
	void     read_request();
	void     select(uint32_t n);
	uint32_t selected();
	void     freeze();
	void     lock();
	uint64_t uid();
	std::string& argument();
};

void handle_request(std::string cmd, std::string arg);
void assert_request(std::string req, json arg, json reply);
void assert_request(std::string req, json arg, json reply, json published);

namespace channel {

void join(u32 ns, std::shared_ptr<session> member);
void leave(u32 ns, std::shared_ptr<session> member);
void publish(u32 ns, std::shared_ptr<session> from, const std::string& msg);
void clear();

} // end of channel namespace

#endif // NSTORE_MOCKS_H

