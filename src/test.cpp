#include "common.h"
#include "db.h"
#include "handler.h"
#include "session.h"

#define CATCH_CONFIG_RUNNER
#include <catch.hpp>

#include <json.hpp>

#include <cstdio>
#include <functional>
#include <memory>
#include <string>

using json = nlohmann::json;

#define db_file "test_db"

int main( int argc, char* argv[] ) {
	logger = spdlog::stdout_color_mt("logger");
	logger->set_pattern("%D %T %L %v");
	return Catch::Session().run( argc, argv );
}


TEST_CASE("operations", "[database]") {
	REQUIRE(db::open(db_file, 20) == false);

	MDB_txn *txn;
	REQUIRE(db::txn_begin(0, &txn) == 0);

	SECTION("meta") {
		REQUIRE(db::put_meta(txn, 0, 42, 777) == false);
		i64 v;
		db::get_meta(txn, 0, 42, &v);
		REQUIRE(v == 777);
	}

	SECTION("exists") {
		REQUIRE(db::exists(txn, 0) == true);
		REQUIRE(db::exists(txn, 1) == false);
	}

	SECTION("setup") {
		REQUIRE(db::exists(txn, 1) == false);
		REQUIRE(db::setup(txn, 1) == false);
		REQUIRE(db::exists(txn, 1) == true);
	}

	mdb_txn_abort(txn);

	db::close();

	REQUIRE(remove(db_file) == 0);
	REQUIRE(remove(db_file "-lock") == 0);
}

TEST_CASE("keys", "[database]") {
	REQUIRE(sizeof(custom_key) == 64);
}

TEST_CASE("sorting", "[database]") {
	auto ikey = [] (u8 s, namespace_t n, entity_t e, attribute_t a, i64 v, transaction_t t) {
		custom_key k = {0};
		k.type = KEY_INTEGER;
		k.sort = s;
		k.ns = n;
		k.e = e;
		k.a = a;
		k.v.i = v;
		k.t = t;
		return k;
	};

	auto c2k = [] (custom_key *c) {
		MDB_val v;
		v.mv_size = sizeof(custom_key);
		v.mv_data = c;
		return v;
	};

	auto compare = [c2k] (custom_key a, custom_key b) {
		auto ka = c2k(&a);
		auto kb = c2k(&b);
		return db::custom_key_compare(&ka, &kb);
	};

	REQUIRE(compare(ikey(SORT_EATV, 0, 1, 2, 3, 4),
			ikey(SORT_EATV, 0, 1, 2, 3, 4)) == 0);

	REQUIRE(compare(ikey(SORT_AETV, 0, 1, 2, 3, 4),
			ikey(SORT_EATV, 0, 1, 2, 3, 4)) != 0);

	u8 sorts[] = { SORT_AETV, SORT_EATV };

	for (auto s : sorts) {
		REQUIRE(compare(ikey(s, 0, 0, 0, 0, 0),
				ikey(s, 0, 0, 0, 0, 0)) == 0);

		REQUIRE(compare(ikey(s, 1, 0, 0, 0, 0),
				ikey(s, 0, 0, 0, 0, 0)) != 0);

		REQUIRE(compare(ikey(s, 0, 1, 0, 0, 0),
				ikey(s, 0, 0, 0, 0, 0)) != 0);

		REQUIRE(compare(ikey(s, 0, 0, 1, 0, 0),
				ikey(s, 0, 0, 0, 0, 0)) != 0);

		REQUIRE(compare(ikey(s, 0, 0, 0, 1, 0),
				ikey(s, 0, 0, 0, 0, 0)) != 0);

		REQUIRE(compare(ikey(s, 0, 0, 0, 0, 1),
				ikey(s, 0, 0, 0, 0, 0)) != 0);
	}
}


struct fixDB {
	bool readonly;
	fixDB(bool readonly) : readonly(readonly) {
		REQUIRE(db::open(db_file, 20) == false);
		if (readonly) {
			REQUIRE(db::txn_begin(MDB_RDONLY, &read_txn) == 0);
			mdb_txn_reset(read_txn);
		}
	}
	~fixDB() {
		if (readonly) mdb_txn_abort(read_txn);
		db::close();
		REQUIRE(remove(db_file) == 0);
		REQUIRE(remove(db_file "-lock") == 0);
	}
};

TEST_CASE("facts", "[handler]") {
	fixDB f(true);
	handle_request("facts", "-1\r");
}

TEST_CASE( "transact", "[handler]" ) {
	fixDB f(false);
	auto s = std::make_shared<session>();
	json arg = {
		// e,  a ,  v , r
		{ -1, 100, 200, 0 },
		{ -1, 101, 201, 0 }
	};
	handle_request("transact", arg.dump());
}

