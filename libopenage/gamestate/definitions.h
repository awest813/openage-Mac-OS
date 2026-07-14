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
 * Whether harvestable resource nodes (e.g. forests, gold) regenerate by default.
 *
 * Disabled keeps the original Age of Empires behaviour: a resource node is
 * removed once it is fully depleted. When enabled (opt-in via the
 * GAMEPLAY_FOREST_REGEN cvar / cfg/gameplay.oac), depleted nodes are kept in
 * the world and slowly regrow back towards the amount they were first tapped at.
 */
constexpr bool FOREST_REGEN_ENABLED_DEFAULT = false;

/**
 * Default seconds of game time between each resource regeneration step for a
 * harvestable resource node when regeneration is enabled.
 */
constexpr double FOREST_REGEN_INTERVAL_SEC = 5.0;

/**
 * Default amount of the resource restored per regeneration step.
 */
constexpr int64_t FOREST_REGEN_AMOUNT = 1;

/**
 * Whether the day/night cycle is enabled by default.
 *
 * Disabled keeps constant daytime sight (original behaviour). When enabled
 * (opt-in via GAMEPLAY_DAY_NIGHT / cfg/gameplay.oac), line-of-sight shrinks
 * at night and during twilight.
 */
constexpr bool DAY_NIGHT_ENABLED_DEFAULT = false;

/**
 * Default length of daytime in seconds of game time.
 */
constexpr double DAY_LENGTH_SEC = 300.0;

/**
 * Default length of nighttime in seconds of game time.
 */
constexpr double NIGHT_LENGTH_SEC = 180.0;

/**
 * Sight-range multipliers for each day phase (applied to DEFAULT_SIGHT_RANGE).
 */
constexpr double DAY_SIGHT_MULT = 1.0;
constexpr double TWILIGHT_SIGHT_MULT = 0.75;
constexpr double NIGHT_SIGHT_MULT = 0.5;

/**
 * Fraction of the day/night segment used as dusk/dawn transition.
 */
constexpr double TWILIGHT_FRACTION = 0.1;

/**
 * Whether dynamic weather is enabled by default.
 *
 * Disabled keeps clear weather (original behaviour). When enabled (opt-in via
 * GAMEPLAY_WEATHER / cfg/gameplay.oac), fog and rain reduce sight and rain
 * slows movement.
 */
constexpr bool WEATHER_ENABLED_DEFAULT = false;

/**
 * Seconds between automatic weather transitions when weather cycling is on.
 */
constexpr double WEATHER_CYCLE_INTERVAL_SEC = 120.0;

/**
 * Sight-range multipliers for weather conditions (stacked with day/night).
 */
constexpr double WEATHER_CLEAR_SIGHT_MULT = 1.0;
constexpr double WEATHER_FOG_SIGHT_MULT = 0.6;
constexpr double WEATHER_RAIN_SIGHT_MULT = 0.8;

/**
 * Movement-speed multipliers for weather conditions.
 */
constexpr double WEATHER_CLEAR_MOVE_MULT = 1.0;
constexpr double WEATHER_FOG_MOVE_MULT = 1.0;
constexpr double WEATHER_RAIN_MOVE_MULT = 0.85;

/**
 * Whether forest hiding is enabled by default.
 *
 * Disabled keeps all fog-visible units visible (original behaviour). When
 * enabled (opt-in via GAMEPLAY_FOREST_HIDE / cfg/gameplay.oac), enemy units
 * standing on forest tiles are invisible beyond FOREST_HIDE_THRESHOLD_TILES.
 */
constexpr bool FOREST_HIDE_ENABLED_DEFAULT = false;

/**
 * Chebyshev distance (tiles) within which a forest-hidden unit remains visible.
 */
constexpr int FOREST_HIDE_THRESHOLD_TILES = 2;

/**
 * Maximum distance (in tiles) for builders to start build or deconstruct actions.
 */
constexpr double BUILDER_INTERACTION_RANGE = 2.0;

/**
 * Phase of the day/night cycle.
 */
enum class day_phase_t : uint8_t {
	DAY = 0,
	DUSK = 1,
	NIGHT = 2,
	DAWN = 3,
};

/**
 * Active weather condition.
 */
enum class weather_t : uint8_t {
	CLEAR = 0,
	FOG = 1,
	RAIN = 2,
};

/**
 * Clamp a resource recovery fraction to [0, 1].
 */
inline double clamp_recovery_fraction(double fraction) {
	return std::clamp(fraction, 0.0, 1.0);
}

} // namespace openage::gamestate
