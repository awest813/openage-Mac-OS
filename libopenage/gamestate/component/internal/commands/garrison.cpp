// Copyright 2026-2026 the openage authors. See copying.md for legal info.

#include "garrison.h"


namespace openage::gamestate::component::command {

GarrisonCommand::GarrisonCommand(entity_id_t target_entity) :
	target_entity{target_entity} {
}

entity_id_t GarrisonCommand::get_target() const {
	return this->target_entity;
}

} // namespace openage::gamestate::component::command
