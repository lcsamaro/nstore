#include "handler.h"
#include "mocks.h"

#include <catch.hpp>

#include <cstring>

std::shared_ptr<spdlog::logger> logger;

int current_pool = POOL_IO;

svc::svc(int pool) : pool(pool) {}
void svc::post(std::function<void()> f) {
	REQUIRE(current_pool != pool);
	current_pool = pool;
	f();
}

svc io_service(POOL_IO);
svc reader_service(POOL_READ);
svc writer_service(POOL_WRITE);


session::session() {}

void session::write(const std::string& msg) {
	REQUIRE(current_pool == POOL_IO);
}

void session::read_request() {
	REQUIRE(current_pool == POOL_IO);
}

void session::select(uint32_t n) {}
uint32_t session::selected() { return 0; }
void session::freeze() {}
void session::lock() {}
uint64_t session::uid() {
	return 0;
}
std::string& session::argument() { return arg; }

void handle_request(std::string cmd, std::string arg) {
	current_pool = POOL_IO;
	auto self = std::make_shared<session>();
	self->arg = arg;
	for (auto& h : handlers) {
		if (strcmp(cmd.c_str(), h.command)) continue;
		auto f = h.fn;
		if (h.flags & HANDLER_WRITE) {
			writer_service.post([f, self] () { f(self); });
		} else if (h.flags & HANDLER_READ) {
			reader_service.post([f, self] () { f(self); });
		} else {
			f(self);
			self->read_request();
		}
		break;
	}
}


namespace channel {

void join(u32 ns, std::shared_ptr<session> member) {
	REQUIRE(current_pool == POOL_IO);
}

void leave(u32 ns, std::shared_ptr<session> member) {
	REQUIRE(current_pool == POOL_IO);
}

void publish(u32 ns, std::shared_ptr<session> from, const std::string& msg) {
	REQUIRE(current_pool == POOL_IO);
}

void clear() {
	REQUIRE(current_pool == POOL_IO);
}

} // end of channel namespace

