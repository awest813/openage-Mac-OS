// Copyright 2024-2024 the openage authors. See copying.md for legal info.

#include "gather.h"


namespace openage::gamestate::component::command {

GatherCommand::GatherCommand(entity_id_t target) :
	target{target} {}

entity_id_t GatherCommand::get_target() const {
	return this->target;
}

} // namespace openage::gamestate::component::command
