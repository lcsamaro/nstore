#ifndef NSTORE_SESSION_H
#define NSTORE_SESSION_H

#ifdef NSTORE_UNIT_TESTING

# include "mocks.h"

#else

#include <asio.hpp>

#include <deque>
#include <string>

using asio::ip::tcp;

class session : public std::enable_shared_from_this<session> {
	tcp::socket s;
	u32 ns;
	u64 id;

	asio::streambuf data;
	std::string arg;

	std::deque<std::string> write_queue;

	void end();
	void do_write();

public:
	session(tcp::socket socket);

	void start();
	void write(const std::string& msg);
	void read_request();

	void     select(u32 n);
	u32 selected();
	u64 uid();
	const char *argument();
};

#endif // NSTORE_UNIT_TESTING

#endif // NSTORE_SESSION_H

