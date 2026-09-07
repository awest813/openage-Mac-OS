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
 * System for repairing damaged buildings and mobile units.
 */
class Repair {
public:
	/**
	 * Dispatches a repair command for an entity.
	 *
	 * Approaches the target if out of range, checks player resources and
	 * damage state, restores HP according to repair speed, and deducts
	 * proportional resources.
	 *
	 * @param entity     The repairing unit.
	 * @param state      The current game state.
	 * @param start_time Current simulation time.
	 * @return Duration of this repair step (or movement time).
	 */
	static const time::time_t repair_command(
	    const std::shared_ptr<gamestate::GameEntity> &entity,
	    const std::shared_ptr<openage::gamestate::GameState> &state,
	    const time::time_t &start_time);
};

} // namespace system
} // namespace gamestate
} // namespace openage
