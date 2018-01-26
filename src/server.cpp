#include "db.h"
#include "channel.h"
#include "session.h"
#include "server.h"

#include <asio.hpp>

#include <cstring>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <functional>
#include <string>
#include <thread>
#include <vector>

std::shared_ptr<spdlog::logger> logger;

asio::io_service io_service;
asio::io_service writer_service;
asio::io_service reader_service;

namespace stats {

time_t startup;
size_t no_connects = 0;
size_t no_disconnects = 0;
size_t no_ro_txns = 0;
size_t no_w_txns = 0;

} // end of stats namespace

server::server(asio::io_service& io_service, short port) :
	a(io_service, tcp::endpoint(tcp::v4(), port)), s(io_service) {
	do_accept();
}

void server::do_accept() {
	a.async_accept(s, [this] (asio::error_code ec) {
		if (!ec) {
			s.set_option(tcp::no_delay(true));
			std::make_shared<session>(std::move(s))->start();
		}
		do_accept();
	});
}

using asio::ip::tcp;

#define IO_LOOP(svc)                             \
	for(;;) {                                \
		try {                            \
			svc.run();               \
			break;                   \
		} catch (std::exception& e) {    \
			logger->error(e.what()); \
		}                                \
	}

void display_usage() {
	puts("usage: nstore [options]");
	puts("options:");
	puts("  --db  <filename>  log file");
	puts("  --log <filename>  log file");
	puts("  --map        <n>  map size (log2)");
	puts("  --port    <port>  listening port");
	puts("  --readers    <n>  reader threadpool size");
	puts("  --time-travel     enable time travel");
	puts("  --tx-log          enable transaction log");
}

int main(int argc, char *argv[]) {
	short port    = 4444;
	int   readers =   8;
	int   sz = 30;
	char *dbfile  = NULL;
	char *logfile = NULL;

	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--")) break;
		if (i+1 < argc) {
			if (!strcmp(argv[i], "--db")) {
				dbfile = argv[++i];
				continue;
			} else if (!strcmp(argv[i], "--port")) {
				if ((port = atoi(argv[++i])) > 0) continue;
				puts("invalid port number");
			} else if (!strcmp(argv[i], "--map")) {
				if ((sz = atoi(argv[++i])) > 0) continue;
				puts("invalid map size");
			} else if (!strcmp(argv[i], "--log")) {
				logfile = argv[++i];
				continue;
			} else if (!strcmp(argv[i], "--readers")) {
				if ((readers = atoi(argv[++i])) > 0) continue;
				puts("invalid number of reader threads");
			}
		}
		display_usage();
		return 1;
	}

	if (!dbfile) {
		puts("db filename not specified");
		display_usage();
		return 1;
	}

	if (readers > 128) readers = 128;

	if (logfile) {
		logger = spdlog::basic_logger_mt("basic_logger", logfile);
	} else {
		logger = spdlog::stdout_color_mt("logger");
	}
	logger->set_pattern("%D %T %L %v");

	stats::startup = std::time(nullptr);

	int rc;
	if ((rc = db::open(dbfile, sz))) {
		logger->error("could not open database");
		logger->error("{}", mdb_strerror(rc));
		return 1;
	}

	auto writer_dummy_work = std::make_shared<asio::io_service::work>(writer_service);
	auto reader_dummy_work = std::make_shared<asio::io_service::work>(reader_service);

	asio::signal_set signals(io_service, SIGINT, SIGTERM);
	signals.async_wait([&] (const asio::error_code& error, int signal_number) {
		logger->info("exiting");
		io_service.stop();
	});

	std::thread writer_thread([&] () { IO_LOOP(writer_service); });

	std::vector<std::thread> reader_threads;
	reader_threads.resize(readers);
	for (int i = 0; i < readers; i++) {
		reader_threads[i] =
			std::thread([] () {
				// initialize thread_local txn
				if (db::txn_begin(MDB_RDONLY, &read_txn)) return;
				mdb_txn_reset(read_txn);
				IO_LOOP(reader_service);
				// finalize thread_local txn
				mdb_txn_commit(read_txn);
			});
	}

	server srv(io_service, port);
	logger->info("server started");
	IO_LOOP(io_service);
	logger->info("io service stopped");
	channel::clear();

	writer_dummy_work.reset();
	writer_thread.join();
	logger->info("writer service stopped");

	reader_dummy_work.reset();
	for (int i = 0; i < readers; i++) reader_threads[i].join();
	logger->info("reader service stopped");

	db::close();
	logger->info("database closed");
	logger->info("bye");
	return 0;
}
