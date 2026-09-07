// Copyright 2026-2026 the openage authors. See copying.md for legal info.

#pragma once

#include <memory>

#include "time/time.h"


namespace openage {

namespace event {
class EventLoop;
} // namespace event

namespace gamestate {
class GameEntity;
class GameState;

namespace system {

/**
 * System for garrisoning units into buildings and vehicles, and ejecting them.
 */
class Garrison {
public:
	/**
	 * Dispatches a garrison command for an entity.
	 *
	 * Approaches the target building if out of range, validates ownership and
	 * capacity, and garrisons the unit upon reaching the building.
	 *
	 * @param entity     The unit attempting to garrison.
	 * @param state      The current game state.
	 * @param start_time Current simulation time.
	 * @return Duration of this step (movement time or 0).
	 */
	static const time::time_t garrison_command(
	    const std::shared_ptr<gamestate::GameEntity> &entity,
	    const std::shared_ptr<openage::gamestate::GameState> &state,
	    const time::time_t &start_time);

	/**
	 * Dispatches an ungarrison command for a building or vehicle.
	 *
	 * Ejects garrisoned units to the rally point or an adjacent unblocked tile.
	 *
	 * @param entity     The building or vehicle containing garrisoned units.
	 * @param state      The current game state.
	 * @param start_time Current simulation time.
	 * @return Duration (0s).
	 */
	static const time::time_t ungarrison_command(
	    const std::shared_ptr<gamestate::GameEntity> &entity,
	    const std::shared_ptr<openage::gamestate::GameState> &state,
	    const time::time_t &start_time);
};

} // namespace system
} // namespace gamestate
} // namespace openage
