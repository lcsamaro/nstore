#define CATCH_CONFIG_MAIN
#include <catch.hpp>

#include "db.h"

/* mocks - begin */



/* mocks - end */
TEST_CASE("operations", "[database]") {
	REQUIRE(db::open("test_db", 20) == false);

	MDB_txn *txn;
	REQUIRE(db::txn_begin(0, &txn) == 0);

	SECTION("meta") {
		REQUIRE (db::put_meta(txn, 0, 42, 777) == false);
		int64_t v;
		db::get_meta(txn, 0, 42, &v);
		REQUIRE(v == 777);
	}

	mdb_txn_abort(txn);

	db::close();
}

TEST_CASE("meta", "[database]") {


}

TEST_CASE("keys", "[database]") {
	REQUIRE(sizeof(custom_key) == 64);
}

TEST_CASE("sorting", "[database]") {
	REQUIRE( 1 == 1 );
}


TEST_CASE("facts", "[handler]") {
	REQUIRE( 1 == 1 );
}

TEST_CASE( "transact", "[handler]" ) {
	REQUIRE( 1 == 1 );
}

