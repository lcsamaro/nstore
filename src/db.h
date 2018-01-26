#ifndef NSTORE_DB_H
#define NSTORE_DB_H

#include <lmdb.h>

#include <string>
#include <vector>

enum sorting { SORT_E=0, SORT_A, SORT_V, SORT_T };
#define MAKE_SORT(s1, s2, s3, s4) (s1 | (s2<<2) | (s3<<4) | (s4<<6))

#define SORT_EATV MAKE_SORT(SORT_E, SORT_A, SORT_T, SORT_V)
#define SORT_AETV MAKE_SORT(SORT_A, SORT_E, SORT_T, SORT_V)
#define SORT_TEAV MAKE_SORT(SORT_T, SORT_E, SORT_A, SORT_V)

#define FLAG_TIME_TRAVEL 0x1
#define FLAG_TX_LOG      0x2

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

typedef int64_t  entity_t;
typedef entity_t attribute_t;
typedef entity_t transaction_t;
typedef uint32_t namespace_t;

extern MDB_dbi dbi;
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

/* new key structure (review):
 * u32 namespace  - 4
 * u64 E + A      - 8
 * u64 tx + index - 8
 * i8  padding    - 1
 * b32 value      - 32
 */

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
	db_ident = 0,
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

// helpers for meta keys
bool put_meta(MDB_txn *txn, namespace_t ns, int64_t id, int64_t val);

bool get_meta(MDB_txn *txn, namespace_t ns, int64_t id, int64_t *val);

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

    int64_t v;
    std::string vs;

    transaction_t t;
    bool r;

    bool is_int;

    datom(int64_t e, int64_t a, int64_t v, int64_t t, bool r) :
            e(e), a(a), v(v), t(t), r(r), is_int(true) {}

    datom(int64_t e, int64_t a, std::string v, int64_t t, bool r) :
            e(e), a(a), vs(v), t(t), r(r), is_int(false) {}
};

typedef std::vector <datom> query_result;

query_result query_a(MDB_txn *txn, namespace_t ns, int64_t tx, int64_t a);
bool last_datom(MDB_txn *txn, custom_key *start);

} // end of db namespace

#endif // NSTORE_DB_H

