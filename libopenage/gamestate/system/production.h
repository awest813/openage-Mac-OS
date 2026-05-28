// Copyright 2026-2026 the openage authors. See copying.md for legal info.

#pragma once

#include <memory>

#include "time/time.h"


namespace openage {

namespace event {
class EventLoop;
}

namespace gamestate {
class GameEntity;
class GameState;

namespace system {

class Production {
public:
	/**
	 * Execute a train command for a game entity (e.g. a building).
	 *
	 * Pops the TRAIN command from the entity's command queue, looks up the
	 * matching creatable in the entity's Create ability, checks that the owner
	 * can afford it, deducts the resource cost, and schedules a
	 * "game.spawn_production" event at the completion time so the finished unit
	 * is spawned by the simulation at the right moment.
	 *
	 * @param entity      Producing game entity.
	 * @param loop        Event loop used to schedule the spawn event.
	 * @param state       Current game state.
	 * @param start_time  Simulation time at which the train action occurs.
	 *
	 * @return Creation (training) time before the unit is produced, or 0 if the
	 *         train could not be performed (unknown creatable or insufficient resources).
	 */
	static const time::time_t train_command(const std::shared_ptr<gamestate::GameEntity> &entity,
	                                         const std::shared_ptr<openage::event::EventLoop> &loop,
	                                         const std::shared_ptr<openage::gamestate::GameState> &state,
	                                         const time::time_t &start_time);
};

} // namespace system
} // namespace gamestate
} // namespace openage
