#ifndef NSTORE_SERVER_H
#define NSTORE_SERVER_H

#include <asio.hpp>

#include <memory>

#include <spdlog/spdlog.h>

extern asio::io_service io_service;
extern asio::io_service writer_service;
extern asio::io_service reader_service;

extern std::shared_ptr<spdlog::logger> logger;

namespace stats {
extern time_t startup;
extern size_t no_connects;
extern size_t no_disconnects;
extern size_t no_ro_txns;
extern size_t no_w_txns;
} // end of stats namespace

class server {
	asio::ip::tcp::acceptor a;
	asio::ip::tcp::socket s;
	void do_accept();
public:
	server(asio::io_service& io_service, short port);
};

#endif // NSTORE_SERVER_H
