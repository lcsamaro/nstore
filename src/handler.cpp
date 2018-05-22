#include "channel.h"
#include "db.h"
#include "handler.h"
#include "server.h"
#include "session.h"
#include "stats.h"

extern "C" {
#include <sodium.h> /* sha-256 */
}

#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/error/en.h>

#include <set>
#include <unordered_map>
#include <unordered_set>

#define EOL "\r\n"
#define ACK_OK  "0" EOL
#define ACK_ERR "1" EOL

#define NEXT_REQ() {                  \
	io_service.post([self] () {   \
		self->read_request(); \
	});                           \
}

#define ERROR(MSG) {          \
	logger->error(MSG);   \
	self->write(ACK_ERR); \
	return;               \
}

#define PERROR(MSG) {                 \
	logger->error(MSG);           \
	io_service.post([self] () {   \
		self->write(ACK_ERR); \
		self->read_request(); \
	});                           \
	return;                       \
}

std::string make_response(rapidjson::Document& res) {
	rapidjson::StringBuffer buffer;
	rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
	res.Accept(writer);
	std::string response = buffer.GetString();
	response.append(EOL);
	return response;
}

void handle_ping(std::shared_ptr<session> self) {
	std::string arg = self->argument();
	arg.append(EOL);
	self->write(arg);
}

void handle_namespace(std::shared_ptr<session> self) {
	rapidjson::Document doc;
	if (doc.Parse(self->argument()).HasParseError()) PERROR("invalid json");
	if (!doc.IsInt64()) PERROR("invalid arg");

	MDB_txn *txn;
	if (db::txn_begin(0, &txn)) PERROR("namespace. txn begin");

	db::setup(txn, doc.GetInt64());

	if (mdb_txn_commit(txn)) PERROR("namespace. txn_commit");

	io_service.post([self] () {
			self->write(ACK_OK);
			self->read_request();
		});
}

void handle_select(std::shared_ptr<session> self) {
	rapidjson::Document doc;
	if (doc.Parse(self->argument()).HasParseError()) PERROR("[select] invalid json");
	if (!doc.IsInt64()) PERROR("[select] invalid arg");
	if (mdb_txn_renew(read_txn)) PERROR("[select] ro txn");
	auto ns = doc.GetInt64();
	auto e = db::exists(read_txn, ns);
	mdb_txn_reset(read_txn);

	if (e) {
		self->select(ns);
		io_service.post([self, ns] () {
				channel::leave(ns, self);
				self->write(ACK_OK);
				self->read_request();
			});
	} else PERROR("[select] namespace does not exist");
}

void handle_facts(std::shared_ptr<session> self) {
	auto arg = self->argument();
	auto ns = self->selected();
	rapidjson::Document doc;
	if (doc.Parse(arg).HasParseError()) PERROR("[facts] invalid json");
	if (!doc.IsInt64()) PERROR("[facts] invalid arg");
	if (mdb_txn_renew(read_txn)) PERROR("[facts] ro txn");

	transaction_t tx = doc.GetInt64();
	if (tx < 0) {
		if (db::get_meta(read_txn, ns, META_TX, &tx)) {
			mdb_txn_reset(read_txn);
			PERROR("[facts] db not initialized");
		}
	}

	// active facts
	MDB_cursor *mc;
	if (mdb_cursor_open(read_txn, dbi, &mc)) { // .GE. than key
		mdb_txn_reset(read_txn);
		PERROR("[facts] cursor_open");
	}

	custom_key min_key = {0};
	min_key.ns   = ns;
	min_key.type = KEY_MIN_SENTINEL;
	min_key.sort = SORT_EATV;

	MDB_val key, data;
	key.mv_data = &min_key;
	key.mv_size = sizeof(custom_key);

	if (mdb_cursor_get(mc, &key, &data, MDB_SET_RANGE)) { // .GE. than key
		mdb_cursor_close(mc);
		mdb_txn_reset(read_txn);
		PERROR("[facts] cursor_get");
	}

	rapidjson::Document res; // Null
	res.SetObject();
	res.AddMember("type", "response", res.GetAllocator());
	res.AddMember("tx", tx, res.GetAllocator());
	rapidjson::Value facts(rapidjson::kArrayType);


	std::set<entity_t> many;
	for (auto& d : db::query_a(read_txn, ns, tx, db_cardinality)) {
		if (d.v == db_cardinality_many) many.insert(d.e);
	}


	tx = (tx<<1) + 1;
	entity_t    last_e = -1;
	attribute_t last_a = -1;

	std::unordered_map<std::string, std::tuple<bool, int64_t, std::string, int64_t>> current;
	auto write_batch = [&] () {
		for (const auto& it : current) {
			rapidjson::Value eavr(rapidjson::kArrayType);
			eavr.PushBack(last_e, res.GetAllocator());
			eavr.PushBack(last_a, res.GetAllocator());

			if (std::get<0>(it.second)) { // string
				auto s = std::get<2>(it.second);
				rapidjson::Value str;
				str.SetString(s.data(), s.size(), res.GetAllocator());
				eavr.PushBack(str, res.GetAllocator());
			} else { // integer
				eavr.PushBack(std::get<1>(it.second), res.GetAllocator());
			}

			eavr.PushBack(std::get<3>(it.second), res.GetAllocator());
			eavr.PushBack(false, res.GetAllocator()); // db add

			facts.PushBack(eavr, res.GetAllocator());
		}
		current.clear();
	};


	int rc = MDB_SUCCESS;
	while (rc == MDB_SUCCESS) {
		custom_key *ck = (custom_key*)key.mv_data;
		std::string v((char*)ck->v.b, 32);

		if (ck->ns != min_key.ns || ck->sort != SORT_EATV) break;
		if (ck->t > tx) goto next;
		if (ck->e != last_e || ck->a != last_a) write_batch();

		if (many.find(ck->a) == many.end()) current.clear(); // cardinality one

		if (ck->t & 1) { // retraction
			current.erase(v);
		} else if (ck->type == KEY_INTEGER) {
			current[v] = std::make_tuple(false, ck->v.i, "", ck->t>>1);
		} else {
			if (ck->pad > 0)
				current[v] = std::make_tuple(true, -1,
					std::string((char*)ck->v.b, ck->pad-1),
					ck->t>>1);
			else
				current[v] = std::make_tuple(true, -1,
					std::string((char*)data.mv_data,
						data.mv_size + ck->pad),
					ck->t>>1);
		}

		last_e = ck->e;
		last_a = ck->a;
next:
		rc = mdb_cursor_get(mc, &key, &data, MDB_NEXT);
	}

	write_batch();

	res.AddMember("facts", facts, res.GetAllocator());

	auto response = make_response(res);
	io_service.post([self, response] () {
		self->write(response);
		self->read_request();
	});

	mdb_cursor_close(mc);
	mdb_txn_reset(read_txn);
}

void handle_transact(std::shared_ptr<session> self) {
	auto arg = self->argument();
	auto ns = self->selected();
	rapidjson::Document doc;
	if (doc.Parse(arg).HasParseError()) PERROR("invalid json");

	if (!doc.IsArray()) PERROR("transact. invalid arg");
	auto facts = doc.GetArray();
	if (!facts.Size()) PERROR("transact. empty fact list");

	MDB_txn *txn;
	if (db::txn_begin(0, &txn)) PERROR("transact. txn begin");

	rapidjson::Document res;
	rapidjson::Value new_facts(rapidjson::kArrayType);
	rapidjson::Value nids(rapidjson::kArrayType);

	std::string response, pub;
	std::unordered_set<entity_t> e_unique;
	std::unordered_map<entity_t, entity_t> new_ids;
	std::unordered_map<entity_t, bool> e_card;
	std::unordered_map<entity_t, bool> e_ref;
	transaction_t tx = 0;
	entity_t id = 0;

	if (db::get_meta(txn, ns, META_TX, &tx)) {
		logger->error("transact: cant get META_TX");
		goto err_tx;
	}
	if (db::get_meta(txn, ns, META_ID, &id)) {
		logger->error("transact: cant get META_ID");
		goto err_tx;
	}

	res.SetObject();
	res.AddMember("type", "response", res.GetAllocator());
	res.AddMember("tx", tx, res.GetAllocator());

	/* queries of interest - begin */
	for (auto& d : db::query_a(txn, ns, tx, db_unique))
		if (d.v == db_unique_yes)
			e_unique.insert(d.e);

	for (auto& d : db::query_a(txn, ns, tx, db_cardinality))
		e_card[d.e] = (d.v == db_cardinality_many);

	for (auto& d : db::query_a(txn, ns, tx, db_type))
		e_ref[d.e] = (d.v == db_type_ref);
	/* queries of interest - end */

	MDB_cursor *mc;
	if (mdb_cursor_open(txn, dbi, &mc)) {
		logger->error("transact: cant open cursor");
		goto err_tx;
	}
	for (auto& v : facts) {
		custom_key ck = {0};
		ck.ns = ns;
		MDB_val val;

		if (!v.IsArray()) {
			logger->info("transact: invalid arg");
			goto err_cur; // invalid arg
		}

		auto eav = v.GetArray();
		if (eav.Size() != 4) {
			logger->info("transact: invalid arg");
			goto err_cur; // invalid arg
		}

		attribute_t a;
		if (eav[1].IsInt64()) {
			a = eav[1].GetInt64();
		} else {
			logger->info("transact: invalid attribute, must be integer");
			goto err_cur; // invalid A
		}

		entity_t e;
		if (eav[0].IsInt64()) {
			e = eav[0].GetInt64();
			if (e < 0) { // tempid
				auto got = new_ids.find(e);
				if (got == new_ids.end()) {
					new_ids[e] = id++;
					e = id-1;
				} else e = got->second;
			} else if (e < db_initial_id) {
				logger->info("transact: cant change basic facts");
				goto err_cur; // can`t change basic facts
			}
		} else {
			logger->info("transact: entity must be integer");
			goto err_cur;
		}

		rapidjson::Value eavr(rapidjson::kArrayType);
		eavr.PushBack(e, res.GetAllocator()).
			PushBack(a, res.GetAllocator());
		if (eav[2].IsInt64()) {
			auto v = eav[2].GetInt64();
			if (e_ref.find(a) != e_ref.end()) { // ref attr
				if (v < 0) { // tempid
					auto got = new_ids.find(v);
					if (got == new_ids.end()) {
						new_ids[v] = id++;
						v = id-1;
					} else v = got->second;
				}
			}
			ck.type = KEY_INTEGER;
			ck.v.i = v;
			eavr.PushBack(ck.v.i, res.GetAllocator());

			val.mv_data = NULL;
			val.mv_size = 0;
		} else if (eav[2].IsString()) { // hash it
			ck.type = KEY_BLOB;

			auto v = eav[2].GetString();
			auto vlen = eav[2].GetStringLength();

			rapidjson::Value str;
			str.SetString(v, vlen, res.GetAllocator());
			eavr.PushBack(str, res.GetAllocator());

			if (vlen > 32) {
				int result = crypto_hash_sha256((unsigned char*)ck.v.b,
					(const unsigned char*)v, vlen);
				if (result) {
					logger->error("transact: sha256 error");
					goto err_cur;
				}

				if (!(vlen&7)) ck.pad = 0;
				else ck.pad = -(8-(vlen & 7));

				val.mv_data = (void*)v;
				val.mv_size = vlen;
			} else {
				memcpy(ck.v.b, v, vlen);
				ck.pad = vlen+1;
				val.mv_data = NULL;
				val.mv_size = 0;
			}
		} else {
			//logger->error("transact: invalid value type");
			logger->warn("[transact] skipping fact, invalid value type");
			continue; //goto err_cur; // DBG
		}

		ck.e = e;
		ck.a = a;

		bool retract = (eav[3].IsInt64() && eav[3].GetInt64());

		// check constraints
		if (e_unique.find(a) != e_unique.end()) { // is unique
			custom_key u_key = ck;
			u_key.ns   = ns;
			u_key.type = KEY_UNIQUE;
			u_key.sort = u_key.pad = u_key.e = u_key.t = 0;

			MDB_val uk;
			uk.mv_size = sizeof(custom_key);
			uk.mv_data = &u_key;

			if (retract) {
				if (mdb_del(txn, dbi, &uk, NULL)) {
					logger->error("transact: cant remove unique key");
					goto err_cur;
				}
			} else {
				custom_key last;
				MDB_val tmp; // shan't modify
				if (!mdb_get(txn, dbi, &uk, &tmp)) {
					entity_t ue;
					memcpy(&ue, tmp.mv_data, sizeof(entity_t));
					if (ue != e) { // another entity with the same AV
						logger->info("unique constraint violated");
						goto err_cur; // unique constraint violated
					} else {
						logger->info("ok, same EA, new V. E:{} A:{}", e, a);
						goto skip_remove_last;
					}
				}

				// now we need to remove last unique AV from entity E from the index
				last = ck;
				if (db::last_datom(txn, &last)) { // find last value of EA
					// err, ok
					logger->info("last value not found! E:{} A:{}", last.e, last.a);
				} else {
					logger->info("removing last value E:{} A:{} T:{}", last.e, last.a, last.t);
					// we have the last value, remove from unique index
					last.ns   = ns;
					last.type = KEY_UNIQUE;
					last.sort = last.pad = last.e = last.t = 0;
					MDB_val l;
					l.mv_size = sizeof(custom_key);
					l.mv_data = &last;
					if (mdb_del(txn, dbi, &l, NULL)) {
						logger->error("transact: cant remove last unique key");
						goto err_cur;
					}
				}
skip_remove_last:
				MDB_val z;
				z.mv_data = &e;
				z.mv_size = sizeof(entity_t);
				if (mdb_put(txn, dbi, &uk, &z, 0)) {
					logger->error("transact: cant put unique key");
					goto err_cur;
				}
			}
		}

		// insert
		eavr.PushBack(tx, res.GetAllocator());
		eavr.PushBack(retract, res.GetAllocator());

		ck.t = (tx << 1) + (retract ? 1 : 0);

		ck.sort = SORT_EATV;
		int er;
		if ((er = db::cursor_put_padded(mc, &ck, &val))) {
			logger->error("transact: error inserting value EATV. code: {}", er);
			goto err_cur;
		}

		if (a < db_initial_id) { // only filled for schema attributes
			ck.sort = SORT_AETV;
			if (db::cursor_put_padded(mc, &ck, &val)) {
				logger->error("transact: error inserting value AETV");
				goto err_cur;
			}
		}

		new_facts.PushBack(eavr, res.GetAllocator());
	}

	mdb_cursor_close(mc);

	// update tx and id
	if (db::put_meta(txn, ns, META_TX, tx+1)) {
		logger->error("transact: cant update META_TX");
		goto err_tx;
	}
        if (db::put_meta(txn, ns, META_ID, id)) {
		logger->error("transact: cant update META_ID");
		goto err_tx;
	}

	if (mdb_txn_commit(txn)) {
		logger->error("transact: cant commit transaction");
		goto err_tx;
	}

	// response
	res.AddMember("facts", new_facts, res.GetAllocator());

	for (auto& it : new_ids) {
		rapidjson::Value kv(rapidjson::kArrayType);
		kv.PushBack(it.first, res.GetAllocator()).
			PushBack(it.second, res.GetAllocator());
		nids.PushBack(kv, res.GetAllocator());
	}
	res.AddMember("new_ids", nids, res.GetAllocator());

	response = make_response(res);

	res.RemoveMember("new_ids");
	res.RemoveMember("type");
	res.AddMember("type", "event", res.GetAllocator());

	pub = make_response(res);
	io_service.post([self, ns, response, pub] () {
		self->write(response);
		channel::publish(ns, self, pub);
		self->read_request();
	});

	return;
err_cur:
	mdb_cursor_close(mc);
err_tx:
	mdb_txn_abort(txn);
	PERROR("transact");
}

