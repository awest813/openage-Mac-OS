// Copyright 2024-2024 the openage authors. See copying.md for legal info.

#pragma once

#include "gamestate/component/api_component.h"
#include "gamestate/component/types.h"


namespace openage::gamestate::component {

/**
 * Component for entities that can perform attacks.
 *
 * Wraps the nyan Attack ability object, which defines damage, range,
 * reload time, and any animated properties.
 */
class Attack final : public APIComponent {
public:
	using APIComponent::APIComponent;

	component_t get_type() const override;
};

} // namespace openage::gamestate::component
