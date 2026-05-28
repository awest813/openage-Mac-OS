// Copyright 2026-2026 the openage authors. See copying.md for legal info.

#pragma once

#include <memory>

#include "time/time.h"


namespace openage::gamestate {
class GameEntity;
class GameState;

namespace system {

/**
 * System that handles guard commands.
 *
 * A guard order makes the entity follow and protect a target entity,
 * attacking any enemies that come within attack range of the guarded unit.
 */
class Guard {
public:
	/**
	 * Execute a guard command for a game entity.
	 *
	 * Pops the GUARD command from the entity's command queue, follows the
	 * target if it strays too far, and attacks enemies in range.
	 *
	 * @param entity     Game entity executing the command.
	 * @param state      Current game state.
	 * @param start_time Simulation time at which the command is processed.
	 *
	 * @return Time until the next event for this entity, or 0 if done / attacking.
	 */
	static const time::time_t guard_command(
		const std::shared_ptr<gamestate::GameEntity> &entity,
		const std::shared_ptr<openage::gamestate::GameState> &state,
		const time::time_t &start_time);
};

} // namespace system
} // namespace openage::gamestate
