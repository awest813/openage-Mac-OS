// Copyright 2024-2024 the openage authors. See copying.md for legal info.

#include "attack.h"

#include "gamestate/component/types.h"


namespace openage::gamestate::component {

component_t Attack::get_type() const {
	return component_t::ATTACK;
}

} // namespace openage::gamestate::component
