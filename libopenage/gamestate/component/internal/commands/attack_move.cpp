// Copyright 2026-2026 the openage authors. See copying.md for legal info.

#include "attack_move.h"


namespace openage::gamestate::component::command {

AttackMoveCommand::AttackMoveCommand(coord::phys3 destination) :
	destination{destination} {}

const coord::phys3 &AttackMoveCommand::get_destination() const {
	return this->destination;
}

} // namespace openage::gamestate::component::command
