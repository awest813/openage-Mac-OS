// Copyright 2026-2026 the openage authors. See copying.md for legal info.

#pragma once

#include "gamestate/component/api_component.h"
#include "gamestate/component/types.h"


namespace openage::gamestate::component {

/**
 * Component for entities (e.g. buildings) that can produce new game entities.
 *
 * Wraps the nyan Create ability object, which defines the set of creatable
 * game entities along with their resource cost and creation (training) time.
 */
class Create final : public APIComponent {
public:
	using APIComponent::APIComponent;

	component_t get_type() const override;
};

} // namespace openage::gamestate::component
