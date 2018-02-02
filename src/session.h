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
	uint32_t ns;
	uint64_t id;

	bool frozen;   // if set, namespace cannot be changed
	bool readonly; // if set, writer operations cannot be done

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

	void     select(uint32_t n);
	uint32_t selected();
	void     freeze();
	void     lock();
	uint64_t uid();
	std::string& argument();
};

#endif // NSTORE_UNIT_TESTING

#endif // NSTORE_SESSION_H

