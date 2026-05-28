// Copyright 2026-2026 the openage authors. See copying.md for legal info.

#include "patrol.h"


namespace openage::gamestate::component::command {

PatrolCommand::PatrolCommand(coord::phys3 waypoint_from, coord::phys3 waypoint_to) :
	waypoint_from{waypoint_from},
	waypoint_to{waypoint_to} {}

const coord::phys3 &PatrolCommand::get_waypoint_from() const {
	return this->waypoint_from;
}

const coord::phys3 &PatrolCommand::get_waypoint_to() const {
	return this->waypoint_to;
}

} // namespace openage::gamestate::component::command
