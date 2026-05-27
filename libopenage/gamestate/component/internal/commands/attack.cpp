// Copyright 2024-2024 the openage authors. See copying.md for legal info.

#include "attack.h"


namespace openage::gamestate::component::command {

AttackCommand::AttackCommand(entity_id_t target) :
	target{target} {}

entity_id_t AttackCommand::get_target() const {
	return this->target;
}

} // namespace openage::gamestate::component::command
