// Copyright 2023-2026 the openage authors. See copying.md for legal info.

#pragma once

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
 * Default population space a unit consumes (and reserves while in production).
 *
 * TODO: source the per-unit population cost from nyan data (a PopulationSpace
 *       ability) once it is available, instead of assuming every unit costs 1.
 */
constexpr int64_t DEFAULT_POPULATION_COST = 1;

/**
 * Hard ceiling on a player's population capacity, regardless of how much
 * headroom their buildings would otherwise provide.
 */
constexpr int64_t POPULATION_MAX = 200;

/**
 * Population headroom each completed building contributes to its owner.
 *
 * TODO: in the original game only some buildings (houses, town centres) raise
 *       the cap, by building-specific amounts. Until a nyan PopulationSpace
 *       ability is wired up, every building (the `OWNERSHIP` + no `MOVE`
 *       heuristic) contributes this same default.
 */
constexpr int64_t DEFAULT_BUILDING_POPULATION_SPACE = 5;

} // namespace openage::gamestate
