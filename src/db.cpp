#include "db.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <iostream>
#include <limits>
#include <tuple>
#include <unordered_map>
#include <unordered_set>

MDB_env *env;
MDB_dbi  dbi;
thread_local MDB_txn *read_txn;

namespace db {

int custom_key_compare(const MDB_val *a, const MDB_val *b) {
	assert(a->mv_size == sizeof(custom_key));
	assert(b->mv_size == sizeof(custom_key));

	custom_key *cka = (custom_key *) a->mv_data;
	custom_key *ckb = (custom_key *) b->mv_data;

	/* assert alignment */
	assert((((uint64_t) cka) & 7) == 0);
	assert((((uint64_t) ckb) & 7) == 0);

	int64_t cmp = cka->ns - ckb->ns;
	if (cmp) return cmp;

	cmp = cka->sort - ckb->sort;
	if (cmp) return cmp;

	if (cka->type == KEY_META || ckb->type == KEY_META) {
		cmp = cka->type - ckb->type;
		if (cmp) return cmp;
		cmp = cka->e - ckb->e;
		if (cmp) return cmp;
	}

	if (cka->type == KEY_UNIQUE || ckb->type == KEY_UNIQUE) {
		cmp = cka->type - ckb->type;
		if (cmp) return cmp;
		cmp = cka->a - ckb->a;
		if (cmp) return cmp;
		return memcmp(cka->v.b, ckb->v.b, 32);
	}

	auto sorting = cka->sort;
	for (int i = 0; i < 4; i++) {
		switch (sorting & 3) {
		case SORT_E: cmp = cka->e - ckb->e; break;
		case SORT_A: cmp = cka->a - ckb->a; break;
		case SORT_T: cmp = cka->t - ckb->t; break;
		case SORT_V:
			cmp = cka->type - ckb->type;
			if (cmp) return cmp;
			switch (cka->type) {
			case KEY_INTEGER:
				cmp = cka->v.i - ckb->v.i;
				break;
			case KEY_BLOB:
				cmp = cka->pad - ckb->pad;
				if (cmp) return cmp;
				cmp = memcmp(cka->v.b, ckb->v.b, 32);
				break;
			}
			break;
		}
		if (cmp) return cmp;
		sorting >>= 2;
	}
	return 0;
}

void fill_meta_key(custom_key *ck, namespace_t ns, int64_t id) {
	ck->type = KEY_META;
	ck->ns = ns;
	ck->e = id;
}

bool put_meta(MDB_txn *txn, namespace_t ns, i64 id, int64_t val) {
	// TODO: store value inside key
	custom_key ck = {0};
	fill_meta_key(&ck, ns, id);
	MDB_val k;
	k.mv_data = &ck;
	k.mv_size = sizeof(custom_key);
	MDB_val v;
	v.mv_data = &val;
	v.mv_size = sizeof(i64);
	return mdb_put(txn, dbi, &k, &v, 0) != 0;
}

bool get_meta(MDB_txn *txn, namespace_t ns, i64 id, i64 *val) {
	custom_key ck = {0};
	fill_meta_key(&ck, ns, id);
	MDB_val k, v;
	k.mv_data = &ck;
	k.mv_size = sizeof(custom_key);
	if (mdb_get(txn, dbi, &k, &v)) return true;
	memcpy(val, v.mv_data, sizeof(int64_t));
	return false;
}

bool exists(MDB_txn *txn, namespace_t ns) {
	transaction_t tx;
	return !get_meta(txn, ns, META_TX, &tx);
}

bool setup(MDB_txn *txn, namespace_t ns) {
	if (exists(txn, ns)) return true;

	if (!ns) {
		if (put_meta(txn, ns, META_VERSION_MAJOR,
				NSTORE_VERSION_MAJOR) ||
			put_meta(txn, ns, META_VERSION_MINOR,
				NSTORE_VERSION_MINOR) ||
			put_meta(txn, ns, META_VERSION_PATCH,
				NSTORE_VERSION_PATCH))
			return true;
	}

	if (put_meta(txn, ns, META_TX, 1)) return true;
	if (put_meta(txn, ns, META_ID, db_initial_id)) return true;

	MDB_cursor *mc;
	if (mdb_cursor_open(txn, dbi, &mc)) return true;

	custom_key ck;
	ck.ns = ns;
	ck.t = 0;

	MDB_val key;
	key.mv_size = sizeof(custom_key);
	key.mv_data = &ck;

	MDB_val val;
	val.mv_data = NULL;
	val.mv_size = 0;

	int rc, flag = 0;
	auto fill_idx = [&] () {
		ck.sort = SORT_EATV;
		rc = mdb_cursor_put(mc, &key, &val, flag);

		ck.sort = SORT_AETV;
		rc = mdb_cursor_put(mc, &key, &val, flag);
	};

	auto ins_int = [&] (entity_t e, attribute_t a, i64 v) {
		ck.type = KEY_INTEGER;
		ck.pad = 0;
		ck.e = e;
		ck.a = a;
		ck.v.i = v;
		fill_idx();
	};

	auto ins_str = [&] (entity_t e, attribute_t a, const char *v) {
		ck.type = KEY_BLOB;
		ck.pad = strlen(v) + 1; // +1, as 0 is actually used for padding
		ck.e = e;
		ck.a = a;
		strncpy((char *) ck.v.b, v, 32);
		fill_idx();
	};

	entity_t unique_e;
	MDB_val val_unique;
	val_unique.mv_data = &unique_e;
	val_unique.mv_size = sizeof(entity_t);

	//         E                A               V
	ins_str(db_ident, db_ident, ":db/ident");
	// reusing ck for unique index here
	unique_e = db_ident;
	ck.type = KEY_UNIQUE;
	ck.sort = ck.e = ck.t = 0;
	rc = mdb_cursor_put(mc, &key, &val_unique, flag);

	ins_str(db_type, db_ident, ":db/type");
	unique_e = db_type;
	ck.type = KEY_UNIQUE;
	ck.sort = ck.e = ck.t = 0;
	rc = mdb_cursor_put(mc, &key, &val_unique, flag);

	ins_str(db_cardinality, db_ident, ":db/cardinality");
	unique_e = db_cardinality;
	ck.type = KEY_UNIQUE;
	ck.sort = ck.e = ck.t = 0;
	rc = mdb_cursor_put(mc, &key, &val_unique, flag);

	ins_str(db_unique, db_ident, ":db/unique");
	unique_e = db_unique;
	ck.type = KEY_UNIQUE;
	ck.sort = ck.e = ck.t = 0;
	rc = mdb_cursor_put(mc, &key, &val_unique, flag);

	ins_int(db_ident,       db_type, db_type_value);
	ins_int(db_type,        db_type, db_type_ref);
	ins_int(db_cardinality, db_type, db_type_ref);
	ins_int(db_unique,      db_type, db_type_ref);

	ins_int(db_ident,       db_cardinality, db_cardinality_one);
	ins_int(db_type,        db_cardinality, db_cardinality_one);
	ins_int(db_cardinality, db_cardinality, db_cardinality_one);
	ins_int(db_unique,      db_cardinality, db_cardinality_one);

	ins_int(db_ident,       db_unique, db_unique_yes);
	ins_int(db_type,        db_unique, db_unique_no);
	ins_int(db_cardinality, db_unique, db_unique_no);
	ins_int(db_unique,      db_unique, db_unique_no);

	ins_str(db_type_value,       db_ident, ":db.type/value");
	ins_str(db_type_ref,         db_ident, ":db.type/ref");
	ins_str(db_cardinality_one,  db_ident, ":db.cardinality/one");
	ins_str(db_cardinality_many, db_ident, ":db.cardinality/many");
	ins_str(db_unique_no,        db_ident, ":db.unique/no");
	ins_str(db_unique_yes,       db_ident, ":db.unique/yes");

	mdb_cursor_close(mc);

	return false; // created
}

int open(const char *name, unsigned long long mapsz) {
	int rc = mdb_env_create(&env);
	if (rc) return rc;

	rc = mdb_env_set_mapsize(env, 1ULL << mapsz); // 1GB
	if (rc) {
		mdb_env_close(env);
		return rc;
	}

	rc = mdb_env_open(env, name,
		      MDB_NOSUBDIR |
		      MDB_WRITEMAP |
		      MDB_NOMETASYNC |
		      MDB_NOSYNC |
		      MDB_MAPASYNC |
		      MDB_NOMEMINIT, 0664);
	if (rc) {
		mdb_env_close(env);
		return rc;
	}

	MDB_txn *txn;
	rc = mdb_txn_begin(env, NULL, 0, &txn);
	if (rc) {
		mdb_env_close(env);
		return rc;
	}

	rc = mdb_dbi_open(txn, NULL, 0, &dbi);
	if (rc) {
		mdb_txn_abort(txn);
		mdb_env_close(env);
		return rc;
	}

	mdb_set_compare(txn, dbi, custom_key_compare);

	setup(txn, 0);

	mdb_txn_commit(txn);

	return 0;
}

void close() {
	mdb_dbi_close(env, dbi);
	mdb_env_close(env);
}

int txn_begin(unsigned int flags, MDB_txn **txn) {
	return mdb_txn_begin(env, NULL, flags, txn);
}

int cursor_open(MDB_txn *txn, MDB_cursor **cursor) {
	return mdb_cursor_open(txn, dbi, cursor);
}

int cursor_put_padded(MDB_cursor *mc, custom_key *key, MDB_val *data) {
	MDB_val k;
	k.mv_size = sizeof(custom_key);
	k.mv_data = key;

	MDB_val v;
	v.mv_size = data->mv_size - key->pad;
	v.mv_data = NULL;

	int rc;
	if (key->pad < 0) {
		if ((rc = mdb_cursor_put(mc, &k, &v, MDB_RESERVE))) return rc;
		memcpy(v.mv_data, data->mv_data, data->mv_size);
		return rc;
	}
	return mdb_cursor_put(mc, &k, data, data->mv_size);
}

bool query(MDB_txn *txn, custom_key *start, std::function<bool(custom_key *k, MDB_val *v)> visitor) {
	MDB_cursor *mc;
	if (mdb_cursor_open(txn, dbi, &mc)) return true;

	MDB_val key, data;
	key.mv_data = start;
	key.mv_size = sizeof(custom_key);

	if (mdb_cursor_get(mc, &key, &data, MDB_SET_RANGE)) { // .GE. than key
		mdb_cursor_close(mc);
		return true;
	}

	int rc = MDB_SUCCESS;
	while (rc == MDB_SUCCESS) {
		if (visitor((custom_key *) key.mv_data, &data)) break;
		rc = mdb_cursor_get(mc, &key, &data, MDB_NEXT);
	}

	mdb_cursor_close(mc);
	return false;
}

datom::datom(entity_t e, attribute_t a, i64 v, transaction_t t, bool r) :
	e(e), a(a), v(v), t(t), r(r), is_int(true) {}

datom::datom(entity_t e, attribute_t a, std::string v, transaction_t t, bool r) :
	e(e), a(a), vs(v), t(t), r(r), is_int(false) {}

query_result query_a(MDB_txn *txn, namespace_t ns, transaction_t tx, attribute_t a) {
	custom_key min_key = {0};
	min_key.ns = ns;
	min_key.type = KEY_MIN_SENTINEL;
	min_key.sort = SORT_AETV;
	min_key.a = a;

	tx = (tx << 1) + 1;

	query_result q;
	std::unordered_map <std::string, std::tuple<bool, int64_t, std::string, int64_t>> current;

	int64_t last_e = -1;
	auto write_batch = [&]() {
		for (const auto &it : current) {
			if (std::get<0>(it.second))
				q.push_back(datom(last_e, a,
					std::get<2>(it.second),
					std::get<3>(it.second), false));
			else
				q.push_back(datom(last_e, a,
					std::get<1>(it.second),
					std::get<3>(it.second), false));
		}
		current.clear();
	};

	auto v = [&](custom_key *k, MDB_val *v) -> bool {
		if (k->a != a || k->ns != ns || k->sort != min_key.sort) return true;
		if (k->t > tx) return false;
		if (k->e != last_e) write_batch();

		std::string vs((char *) k->v.b, 32);
		if (k->t & 1) { // retraction
			current.erase(vs);
		} else if (k->type == KEY_INTEGER) {
			current[vs] = std::make_tuple(false, k->v.i, "", k->t >> 1);
		} else {
			if (k->pad > 0)
				current[vs] = std::make_tuple(true, -1,
					std::string((char *) k->v.b, k->pad - 1),
					k->t >> 1);
			else
				current[vs] = std::make_tuple(true, -1,
					std::string((char *) v->mv_data,
					v->mv_size + k->pad),
					k->t >> 1);
		}

		last_e = k->e;
		return false;
	};

	query(txn, &min_key, v);
	write_batch();

	return q;
}

bool last_datom(MDB_txn *txn, custom_key *start) {
	MDB_cursor *mc;
	if (mdb_cursor_open(txn, dbi, &mc)) return true;

	auto e = start->e;
	auto a = start->a;
	auto ns = start->ns;

	start->sort = SORT_EATV;
	start->a++;
	start->t = -1;

	MDB_val key, data;
	key.mv_data = start;
	key.mv_size = sizeof(custom_key);

	if (mdb_cursor_get(mc, &key, &data, MDB_SET_RANGE) || // .GE. than key
		mdb_cursor_get(mc, &key, &data, MDB_PREV)) {
		mdb_cursor_close(mc);
		return true;
	}

	memcpy(start, key.mv_data, sizeof(custom_key));

	mdb_cursor_close(mc);
	return (e != start->e || a != start->a || ns != start->ns);
}

} // end of db namespace

