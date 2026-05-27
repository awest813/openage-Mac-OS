// Copyright 2024-2024 the openage authors. See copying.md for legal info.

#pragma once

#include "gamestate/component/api_component.h"
#include "gamestate/component/types.h"


namespace openage::gamestate::component {

/**
 * Component for entities that can gather resources.
 *
 * Wraps the nyan Gather ability object, which defines the gather rate,
 * resource type, max range, and reload time between gather actions.
 */
class Gather final : public APIComponent {
public:
	using APIComponent::APIComponent;

	component_t get_type() const override;
};

} // namespace openage::gamestate::component
