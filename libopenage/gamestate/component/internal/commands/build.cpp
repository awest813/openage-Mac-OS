// Copyright 2026-2026 the openage authors. See copying.md for legal info.

#include "build.h"


namespace openage::gamestate::component::command {

BuildCommand::BuildCommand(const std::string &building, const coord::phys3 &target) :
	building{building},
	target{target} {}

const std::string &BuildCommand::get_building() const {
	return this->building;
}

const coord::phys3 &BuildCommand::get_target() const {
	return this->target;
}

} // namespace openage::gamestate::component::command
