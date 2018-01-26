#ifndef NSTORE_HANDLER_H
#define NSTORE_HANDLER_H

#include <memory>

class session;

enum handler_flags {
	HANDLER_IO    = 0x0,
	HANDLER_READ  = 0x1,
	HANDLER_WRITE = 0x2,
	HANDLER_LOCK  = 0x4
};

struct handler {
	const char* command;
	void (*fn) (std::shared_ptr<session>);
	int flags;
};

const int no_handlers = 10;
extern handler handlers[no_handlers];

#endif // NSTORE_HANDLER_H

