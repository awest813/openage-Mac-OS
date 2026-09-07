// Copyright 2026-2026 the openage authors. See copying.md for legal info.

#include "repair.h"


namespace openage::gamestate::component::command {

RepairCommand::RepairCommand(entity_id_t target_entity) :
	target_entity{target_entity} {
}

entity_id_t RepairCommand::get_target() const {
	return this->target_entity;
}

} // namespace openage::gamestate::component::command
