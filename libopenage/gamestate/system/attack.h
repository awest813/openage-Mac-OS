// Copyright 2024-2024 the openage authors. See copying.md for legal info.

#pragma once

#include <memory>

#include "gamestate/types.h"
#include "time/time.h"


namespace openage::gamestate {
class GameEntity;
class GameState;

namespace system {

class Attack {
public:
	/**
	 * Execute an attack command for a game entity.
	 *
	 * Pops the ATTACK command from the entity's command queue, looks up
	 * the target in the game state, and applies damage to it.
	 *
	 * @param entity  Attacking game entity.
	 * @param state   Current game state (used to look up the target entity).
	 * @param start_time  Simulation time at which the attack occurs.
	 *
	 * @return Reload time after the attack (i.e. how long until the entity
	 *         can attack again), or 0 if the attack could not be performed.
	 */
	static const time::time_t attack_command(const std::shared_ptr<gamestate::GameEntity> &entity,
	                                         const std::shared_ptr<openage::gamestate::GameState> &state,
	                                         const time::time_t &start_time);

	/**
	 * Apply an attack from one entity to a specific target.
	 *
	 * @param attacker  Entity performing the attack.
	 * @param target_id ID of the entity being attacked.
	 * @param state     Current game state.
	 * @param start_time Simulation time of the attack.
	 *
	 * @return Reload time after the attack, or 0 if the attack could not
	 *         be performed.
	 */
	static const time::time_t attack_default(const std::shared_ptr<gamestate::GameEntity> &attacker,
	                                         entity_id_t target_id,
	                                         const std::shared_ptr<openage::gamestate::GameState> &state,
	                                         const time::time_t &start_time);
};

} // namespace system
} // namespace openage::gamestate
