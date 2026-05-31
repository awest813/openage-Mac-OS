// Copyright 2026-2026 the openage authors. See copying.md for legal info.

#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include <nyan/nyan.h>

#include "gamestate/definitions.h"

namespace openage::gamestate {
struct BuildingCostRecord;
}


namespace nyan {
class View;
}

namespace openage::gamestate::api {

/**
 * Resolved data from a CreatableGameEntity entry on a Create ability.
 */
struct CreatableCostInfo {
	bool found = false;
	std::string game_entity;
	std::string cost_resource;
	int64_t cost_amount = 0;
	double creation_time = 0.0;
	/**
	 * Seconds for a villager to deconstruct; 0 means use creation_time *
	 * DECONSTRUCT_TIME_FACTOR.
	 */
	double deconstruct_time = 0.0;
	double salvage_recovery_fraction = SALVAGE_RECOVERY_FRACTION;
	double deconstruct_recovery_fraction = DECONSTRUCT_RECOVERY_FRACTION;
};

/**
 * Find a creatable on a Create ability matching \p target_game_entity.
 */
CreatableCostInfo lookup_creatable(const std::shared_ptr<nyan::View> &db_view,
                                   const nyan::Object &create_ability,
                                   const std::string &target_game_entity);

/**
 * Build a \p BuildingCostRecord from resolved creatable data.
 */
openage::gamestate::BuildingCostRecord building_cost_from_creatable(const CreatableCostInfo &info);

} // namespace openage::gamestate::api
