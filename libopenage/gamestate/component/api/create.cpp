// Copyright 2026-2026 the openage authors. See copying.md for legal info.

#include "create.h"

#include "gamestate/component/types.h"


namespace openage::gamestate::component {

component_t Create::get_type() const {
	return component_t::CREATE;
}

} // namespace openage::gamestate::component
