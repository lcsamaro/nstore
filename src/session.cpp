#include "channel.h"
#include "db.h"
#include "handler.h"
#include "server.h"
#include "session.h"
#include "stats.h"

#include <cctype>
#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

#define MAYBE_QUIT() if (ec) { end(); return; }
#define CMD_DELIM ' '
#define EOL "\r\n"
#define ACK_OK  "0" EOL
#define ACK_ERR "1" EOL

static uint64_t next_uid = 0;

session::session(tcp::socket socket) :
	s(std::move(socket)), ns(0),
	frozen(false), readonly(false) {
	id = next_uid++;
	stats::no_connects++;
}

void session::end() {
	stats::no_disconnects++;
	channel::leave(ns, shared_from_this());
}

void session::start() {
	auto self(shared_from_this());
	channel::join(0, self);
	read_request();
}

// io thread
void session::write(const std::string& msg) {
	bool write_in_progress = !write_queue.empty();
	write_queue.push_back(msg);
	if (!write_in_progress) do_write();
}

void session::select(uint32_t n) {
	ns = n;
}

uint32_t session::selected() {
	return ns;
}

void session::freeze() {
	frozen = true;
}

void session::lock() {
	readonly = true;
}

uint64_t session::uid() {
	return id;
}

std::string& session::argument() {
	return arg;
}

void session::do_write() {
	auto self(shared_from_this());
	asio::async_write(s,
		asio::buffer(write_queue.front().data(), write_queue.front().length()),
		[this, self] (std::error_code ec, std::size_t length) {
			MAYBE_QUIT();
			write_queue.pop_front();
			if (!write_queue.empty()) do_write();
		});
}

void session::read_request() {
	auto self(shared_from_this());
	asio::async_read_until(s, data, EOL,
		[this, self] (asio::error_code ec, size_t length) {
			MAYBE_QUIT();
			std::string cmd;
			std::istream stream(&data);
			std::getline(stream, cmd, CMD_DELIM);
			std::getline(stream, arg);

			for (auto& h : handlers) {
				if (strcmp(cmd.c_str(), h.command)) continue;
				if ((h.flags & HANDLER_LOCK) && frozen) goto error;
				auto f = h.fn;
				if (h.flags & HANDLER_WRITE) {
					if (readonly) goto error;
					stats::no_w_txns++;
					writer_service.post([f, self] () { f(self); });
				} else if (h.flags & HANDLER_READ) {
					stats::no_ro_txns++;
					reader_service.post([f, self] () { f(self); });
				} else { // IO
					f(self);
					read_request();
				}
				break;
			}

			return;
error:
			write(ACK_ERR);
			read_request();
		});
}

