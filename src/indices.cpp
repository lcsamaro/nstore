#include "db.h"

#include <chrono>
#include <iostream>
#include <string>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

static const char hexc[] = "0123456789abcdef";

static void hex(unsigned char c) {
	putchar(hexc[c >> 4]);
	putchar(hexc[c & 0xf]);
}

static void text(void *data, int sz) {
	unsigned char *c, *end;
	c = (unsigned char*)data;
	end = c + sz;
	while (c < end) {
		if (isprint(*c)) {
			putchar(*c);
		} else {
			putchar('\\');
			hex(*c);
		}
		c++;
	}
}

static void hexify(void *data, int sz) {
	unsigned char *c, *end;
	c = (unsigned char*)data;
	end = c + sz;
	while (c < end) {
		hex(*c);
		c++;
	}
}


const std::string black   ("\033[0;30m");
const std::string red     ("\033[1;31m");
const std::string green   ("\033[1;32m");
const std::string yellow  ("\033[1;33m");
const std::string blue    ("\033[1;34m");
const std::string magenta ("\033[0;35m");
const std::string cyan    ("\033[0;36m");
const std::string white   ("\037[0;37m");
const std::string reset   ("\033[0m");

void print_custom_key(const MDB_val *k) {
	custom_key *ck = (custom_key*)k->mv_data;
	printf("%010d ", ck->ns);
	//printf("%03d ", ck->sort);
	//printf("%02d ", ck->type);
	printf("%02d ", ck->pad);
	switch (ck->type) {
	case KEY_MIN_SENTINEL: printf("min:"); break;
	case KEY_META: printf("M:%.*s ", 32, ck->v.b); return;
	case KEY_INTEGER: printf("I:"); break;
	case KEY_BLOB: printf("B:"); break;
	case KEY_UNIQUE: printf("U:%.*s ", 32, ck->v.b); return;
	default: /* should never happen! */
		printf("invalid key %d\n", ck->type);
		text(k->mv_data, k->mv_size);
		puts("\n");
	       return;
	}

	auto sorting = ck->sort;
	for (int i = 0; i < 4; i++) {
		switch (sorting & 3) {
		case SORT_E: std::cout << red << "E" << reset; break;
		case SORT_A: std::cout << green << "A" << reset; break;
		case SORT_V: std::cout << blue << "V" << reset; break;
		case SORT_T: std::cout << yellow << "T" << reset;
		}
		sorting >>= 2;
	}
	printf(":");

	std::cout << red << ck->e << reset << ':';
	std::cout << green <<  ck->a << reset << ':';

	std::cout << blue;
	switch (ck->type) {
	case KEY_MIN_SENTINEL: std::cout << "MIN"; break;
	case KEY_INTEGER: std::cout << ck->v.i; break;
	case KEY_BLOB: {
		if (ck->pad > 0) {
			text(ck->v.b, ck->pad-1);
		} else {
			hexify(ck->v.b, 32);
		}
	} break;
	default: break;
	}
	std::cout << reset << ':' << yellow << ck->t << reset << ' ';
}

void dump() {
	MDB_txn *txn;
	int rc = db::txn_begin(MDB_RDONLY, &txn);
	if (rc) {
		puts("failed to open txn");
		return;
	}
	MDB_cursor *mc;
	rc = db::cursor_open(txn, &mc);
	if (rc) {
		puts("failed to open cursor");
		mdb_txn_abort(txn);
		return;
	}
	MDB_val key, data;
	while ((rc = mdb_cursor_get(mc, &key, &data, MDB_NEXT) == MDB_SUCCESS)) {
		//custom_key ck;
		//memcpy(&ck, key.mv_data, sizeof(custom_key));
		print_custom_key(&key);
		auto pad = ((custom_key*)key.mv_data)->pad;
		if (pad < 0) text(data.mv_data, data.mv_size + pad);
		else text(data.mv_data, data.mv_size);
		printf("\n");
	}
	mdb_cursor_close(mc);
	mdb_txn_commit(txn);
}

int main(int argc, char *argv[]) {
	if (argc < 2) return 1;
	if (db::open(argv[1], 32)) return 1;
	dump();
	db::close();
	return 0;
}

