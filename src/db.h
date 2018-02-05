#ifndef NSTORE_DB_H
#define NSTORE_DB_H

#include "common.h"

#include <lmdb.h>

#include <string>
#include <vector>

enum sorting { SORT_E=0, SORT_A, SORT_V, SORT_T };
#define MAKE_SORT(s1, s2, s3, s4) (s1 | (s2<<2) | (s3<<4) | (s4<<6))

#define SORT_EATV MAKE_SORT(SORT_E, SORT_A, SORT_T, SORT_V)
#define SORT_AETV MAKE_SORT(SORT_A, SORT_E, SORT_T, SORT_V)

enum {
	INDEX_META,
	INDEX_TXN,
	INDEX_EATV,
	INDEX_AETV
};

enum {
    META_TX = 0,
    META_ID
};

typedef i64      entity_t;
typedef entity_t attribute_t;
typedef entity_t transaction_t;
typedef u32      namespace_t;

extern              MDB_dbi  dbi;
extern thread_local MDB_txn *read_txn;

enum {
	KEY_META = 0,
	KEY_UNIQUE,
	KEY_MIN_SENTINEL,
	KEY_INTEGER,
	KEY_BLOB,
	KEY_MAX_SENTINEL
};

union value {
	int64_t i;
	double  f;
	int8_t  b[32]; // 32b
};

struct custom_key {
	namespace_t   ns; // 4b
	uint8_t     type; // 1b
	uint8_t     sort; // 1b

	// value padding, as we want every key aligned at 8 bytes
	// <= 0 -> -padding
	// >  0 -> value_size-1
	int16_t      pad; // 2b

	entity_t       e; // 8b
	attribute_t    a; // 8b
	transaction_t  t; // 8b
	value          v; // 32b
};

// basic facts
enum {
	db_nil = 0,
	db_ident,
	db_type,
	db_cardinality,
	db_unique,

	db_type_value,
	db_type_ref,

	db_cardinality_one,
	db_cardinality_many,

	db_unique_no,
	db_unique_yes,

	db_initial_id
};

namespace db {

int custom_key_compare(const MDB_val *a, const MDB_val *b);

// helpers for meta keys
bool put_meta(MDB_txn *txn, namespace_t ns, i64 id, i64 val);

bool get_meta(MDB_txn *txn, namespace_t ns, i64 id, i64 *val);

// check if db namespace exists
bool exists(MDB_txn *txn, namespace_t ns);

// setup db namespace basic facts/meta
bool setup(MDB_txn *txn, namespace_t ns);

// open lmdb database
int open(const char *name, unsigned long long mapsz);

// close lmdb database
void close();

int txn_begin(unsigned int flags, MDB_txn **txn);
int cursor_open(MDB_txn *txn, MDB_cursor **cursor);
int cursor_put_padded(MDB_cursor *mc, custom_key *key, MDB_val *data);

// query
struct datom {
    entity_t e;
    attribute_t a;

    i64 v;
    std::string vs;

    transaction_t t;
    bool r;
    bool is_int;

    datom(entity_t e, attribute_t a, i64 v, transaction_t t, bool r);
    datom(entity_t e, attribute_t a, std::string v, transaction_t t, bool r);
};

typedef std::vector <datom> query_result;

query_result query_a(MDB_txn *txn, namespace_t ns, transaction_t tx, attribute_t a);
bool last_datom(MDB_txn *txn, custom_key *start);

} // end of db namespace

#endif // NSTORE_DB_H

