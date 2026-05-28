// Copyright 2026-2026 the openage authors. See copying.md for legal info.

#pragma once

#include <memory>

#include "time/time.h"


namespace openage::gamestate {
class GameEntity;
class GameState;

namespace system {

/**
 * System that handles attack-move commands.
 *
 * An attack-move order moves the entity toward a destination, attacking any
 * enemies encountered along the way.
 */
class AttackMove {
public:
	/**
	 * Execute an attack-move command for a game entity.
	 *
	 * Pops the ATTACK_MOVE command from the entity's command queue, scans for
	 * nearby enemies, and either attacks the closest one in range or advances
	 * toward the destination.
	 *
	 * @param entity     Game entity executing the command.
	 * @param state      Current game state.
	 * @param start_time Simulation time at which the command is processed.
	 *
	 * @return Time until the next event for this entity, or 0 if done / attacking.
	 */
	static const time::time_t attack_move_command(
		const std::shared_ptr<gamestate::GameEntity> &entity,
		const std::shared_ptr<openage::gamestate::GameState> &state,
		const time::time_t &start_time);
};

} // namespace system
} // namespace openage::gamestate
