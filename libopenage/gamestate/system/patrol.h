// Copyright 2026-2026 the openage authors. See copying.md for legal info.

#pragma once

#include <memory>

#include "time/time.h"


namespace openage::gamestate {
class GameEntity;
class GameState;

namespace system {

/**
 * System that handles patrol commands.
 *
 * A patrol order moves the entity back and forth between two waypoints,
 * attacking any enemies encountered along the way.
 */
class Patrol {
public:
	/**
	 * Execute a patrol command for a game entity.
	 *
	 * Pops the PATROL command from the entity's command queue, moves the
	 * entity toward the current waypoint, reverses direction upon arrival,
	 * and attacks enemies in range.
	 *
	 * @param entity     Game entity executing the command.
	 * @param state      Current game state.
	 * @param start_time Simulation time at which the command is processed.
	 *
	 * @return Time until the next event for this entity, or 0 if done / attacking.
	 */
	static const time::time_t patrol_command(
		const std::shared_ptr<gamestate::GameEntity> &entity,
		const std::shared_ptr<openage::gamestate::GameState> &state,
		const time::time_t &start_time);
};

} // namespace system
} // namespace openage::gamestate
