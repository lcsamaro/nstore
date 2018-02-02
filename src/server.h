#ifndef NSTORE_SERVER_H
#define NSTORE_SERVER_H

#include <asio.hpp>

#include <memory>

#ifdef NSTORE_UNIT_TESTING
# include "mocks.h"
#else
# include <spdlog/spdlog.h>
extern std::shared_ptr<spdlog::logger> logger;

extern asio::io_service io_service;
extern asio::io_service writer_service;
extern asio::io_service reader_service;
#endif // NSTORE_UNIT_TESTING

class server {
	asio::ip::tcp::acceptor a;
	asio::ip::tcp::socket s;
	void do_accept();
public:
	server(asio::io_service& io_service, short port);
};

#endif // NSTORE_SERVER_H
