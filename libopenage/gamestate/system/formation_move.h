// Copyright 2026-2026 the openage authors. See copying.md for legal info.

#pragma once

#include <memory>

#include "time/time.h"


namespace openage::gamestate {
class GameEntity;
class GameState;

namespace system {

/**
 * System that handles formation-move commands.
 *
 * A formation-move command carries a group centroid destination and this
 * unit's personal offset from that centroid.  The system resolves the final
 * per-unit destination as `centroid + offset` and delegates to
 * Move::move_default.
 */
class FormationMove {
public:
	/**
	 * Process a FORMATION_MOVE command for a game entity.
	 *
	 * @param entity     Game entity executing the command.
	 * @param state      Game state.
	 * @param start_time Simulation time at which the command starts.
	 *
	 * @return Runtime of the movement in simulation time.
	 */
	static const time::time_t formation_move_command(
		const std::shared_ptr<gamestate::GameEntity> &entity,
		const std::shared_ptr<openage::gamestate::GameState> &state,
		const time::time_t &start_time);
};

} // namespace system
} // namespace openage::gamestate
