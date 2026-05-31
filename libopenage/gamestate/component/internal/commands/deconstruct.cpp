// Copyright 2026-2026 the openage authors. See copying.md for legal info.

#include "deconstruct.h"


namespace openage::gamestate::component::command {

DeconstructCommand::DeconstructCommand(entity_id_t target_building) :
	target_building{target_building} {
}

entity_id_t DeconstructCommand::get_target() const {
	return this->target_building;
}

} // namespace openage::gamestate::component::command
