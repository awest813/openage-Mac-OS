// Copyright 2026-2026 the openage authors. See copying.md for legal info.

#include "guard.h"


namespace openage::gamestate::component::command {

GuardCommand::GuardCommand(entity_id_t target_id) :
	target_id{target_id} {}

entity_id_t GuardCommand::get_target_id() const {
	return this->target_id;
}

} // namespace openage::gamestate::component::command
