#ifndef NSTORE_HANDLER_H
#define NSTORE_HANDLER_H

#include <memory>

class session;

void handle_ping(std::shared_ptr<session> self);
void handle_namespace(std::shared_ptr<session> self);
void handle_select(std::shared_ptr<session> self);
void handle_facts(std::shared_ptr<session> self);
void handle_transact(std::shared_ptr<session> self);

#endif // NSTORE_HANDLER_H

