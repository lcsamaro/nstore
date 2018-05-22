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

static u64 next_uid = 0;

session::session(tcp::socket socket) :
	s(std::move(socket)), ns(0) {
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

void session::select(u32 n) {
	ns = n;
}

u32 session::selected() {
	return ns;
}

u64 session::uid() {
	return id;
}

const char *session::argument() {
	return arg.c_str()+1;
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
			std::istream stream(&data);
			std::getline(stream, arg);
			switch (arg[0]) {
			case 'p':
				handle_ping(self);
				read_request();
				break;
			case 'n':
				writer_service.post([self] () { handle_namespace(self); });
				break;
			case 's':
				reader_service.post([self] () { handle_select(self); });
				break;
			case 'f':
				reader_service.post([self] () { handle_facts(self); });
				break;
			case 't':
				writer_service.post([self] () { handle_transact(self); });
				break;
			default:
				write(ACK_ERR);
				read_request();
			}
		});
}

