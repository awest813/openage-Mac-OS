// Copyright 2026-2026 the openage authors. See copying.md for legal info.

#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <nyan/nyan.h>

#include <optional>

#include "event/eventhandler.h"
#include "gamestate/definitions.h"
#include "gamestate/game_state.h"

namespace nyan {
class View;
}

namespace openage::gamestate {
class Player;
}

namespace openage::gamestate::api {

/**
 * Resolved data from a CreatableGameEntity entry on a Create ability.
 */
struct CreatableCostInfo {
	bool found = false;
	std::string game_entity;
	std::vector<ResourceCostEntry> cost_entries;
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

/**
 * Normalize recovery fractions and deconstruct time on a building cost record.
 */
openage::gamestate::BuildingCostRecord normalize_building_cost(
    openage::gamestate::BuildingCostRecord cost);

/**
 * Parse a building cost record from spawn-production event parameters, if present.
 */
std::optional<openage::gamestate::BuildingCostRecord> building_cost_from_event_params(
    const openage::event::EventHandler::param_map &params);

/**
 * Return true if the player can afford every entry in \p cost at \p time.
 */
bool player_can_afford(const openage::gamestate::Player &player,
                       const openage::gamestate::BuildingCostRecord &cost,
                       const time::time_t &time);

/**
 * Deduct every entry in \p cost from the player's resources at \p time.
 */
void player_pay_cost(openage::gamestate::Player &player,
                     const openage::gamestate::BuildingCostRecord &cost,
                     const time::time_t &time);

} // namespace openage::gamestate::api
