// Copyright 2023-2026 the openage authors. See copying.md for legal info.

#pragma once

#include <memory>

#include "time/time.h"


namespace openage::gamestate {
class GameEntity;
class GameState;

namespace system {

class Idle {
public:
	/**
	 * Let a game entity idle.
	 *
	 * Plays the idle animation. If the entity has a STANCE component that
	 * allows auto-attack (AGGRESSIVE or DEFENSIVE), scans nearby enemies and
	 * pushes an ATTACK command if one is in range.
	 *
	 * @param entity     Game entity.
	 * @param state      Current game state (used for enemy scanning).
	 * @param start_time Start time of change.
	 *
	 * @return Runtime of the change in simulation time (always 0).
	 */
	static const time::time_t idle(const std::shared_ptr<gamestate::GameEntity> &entity,
	                               const std::shared_ptr<openage::gamestate::GameState> &state,
	                               const time::time_t &start_time);
};

} // namespace system
} // namespace openage::gamestate
