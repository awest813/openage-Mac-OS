// Copyright 2023-2026 the openage authors. See copying.md for legal info.

#pragma once

#include <algorithm>
#include <cstdint>

#include "coord/phys.h"

/**
 * Hardcoded definitions for parameters used in the gamestate.
 *
 * May be moved to configuration files in the future.
 */
namespace openage::gamestate {

/**
 * Origin point of the game world.
 */
constexpr coord::phys3 WORLD_ORIGIN = coord::phys3{0, 0, 0};

/**
 * Fallback population demand when an entity has no recorded or nyan-sourced
 * value (e.g. test helpers that bypass SpawnProductionHandler).
 */
constexpr int64_t DEFAULT_POPULATION_COST = 1;

/**
 * Hard ceiling on a player's population capacity, regardless of how much
 * headroom their buildings would otherwise provide.
 */
constexpr int64_t POPULATION_MAX = 200;

/**
 * Fallback population headroom when a building has no recorded or nyan-sourced
 * ProvideContingent value (e.g. test helpers that bypass SpawnProductionHandler).
 */
constexpr int64_t DEFAULT_BUILDING_POPULATION_SPACE = 5;

/**
 * Default fraction of construction cost returned as salvage when a building is
 * destroyed (combat, delete, etc.). Overridden per creatable via nyan
 * CreatableGameEntity.salvage_recovery_fraction when set.
 */
constexpr double SALVAGE_RECOVERY_FRACTION = 0.5;

/**
 * Default fraction recovered when a building is deconstructed (slower, higher yield).
 * Overridden per creatable via CreatableGameEntity.deconstruct_recovery_fraction.
 */
constexpr double DECONSTRUCT_RECOVERY_FRACTION = 0.75;

/**
 * Multiplier applied to creation_time when CreatableGameEntity.deconstruct_time is unset.
 */
constexpr double DECONSTRUCT_TIME_FACTOR = 1.5;

/**
 * Seconds of game time between each unit of salvage decay (1 resource / interval).
 */
constexpr double SALVAGE_DECAY_INTERVAL_SEC = 10.0;

/**
 * Maximum distance (in tiles) for builders to start build or deconstruct actions.
 */
constexpr double BUILDER_INTERACTION_RANGE = 2.0;

/**
 * Clamp a resource recovery fraction to [0, 1].
 */
inline double clamp_recovery_fraction(double fraction) {
	return std::clamp(fraction, 0.0, 1.0);
}

} // namespace openage::gamestate
