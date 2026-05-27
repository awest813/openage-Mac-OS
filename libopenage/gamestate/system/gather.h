// Copyright 2024-2024 the openage authors. See copying.md for legal info.

#pragma once

#include <memory>

#include "time/time.h"


namespace openage::gamestate {
class GameEntity;
class GameState;

namespace system {

class Gather {
public:
	/**
	 * Execute a gather command for a game entity.
	 *
	 * Pops the GATHER command from the entity's command queue, looks up
	 * the resource entity in the game state, and extracts resources from it,
	 * crediting the gathering entity's owner player.
	 *
	 * @param entity  Gathering game entity.
	 * @param state   Current game state (used to look up the resource entity and owner).
	 * @param start_time  Simulation time at which the gather action occurs.
	 *
	 * @return Reload time after the gather action (i.e. how long until the entity
	 *         can gather again), or 0 if the gather could not be performed.
	 */
	static const time::time_t gather_command(const std::shared_ptr<gamestate::GameEntity> &entity,
	                                         const std::shared_ptr<openage::gamestate::GameState> &state,
	                                         const time::time_t &start_time);
};

} // namespace system
} // namespace openage::gamestate
