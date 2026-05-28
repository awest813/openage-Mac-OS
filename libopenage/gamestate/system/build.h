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

/**
 * System for handling BUILD commands.
 *
 * A build command causes a builder entity to walk to the target position
 * and then construct a building.  The builder first moves to the build
 * site (via Move::move_default); once there, the system verifies the
 * owner can afford the building, deducts the resource cost, and schedules
 * a "game.spawn_production" event at the completion time.  The same
 * SpawnProductionHandler used for unit training handles the actual spawn.
 */
class Build {
public:
	/**
	 * Execute a build command for a game entity (e.g. a villager).
	 *
	 * @param entity      Builder game entity.
	 * @param loop        Event loop used to schedule the spawn event.
	 * @param state       Current game state.
	 * @param start_time  Simulation time at which the build action occurs.
	 *
	 * @return Construction time before the building is placed, or 0 if the
	 *         build could not be performed.
	 */
	static const time::time_t build_command(const std::shared_ptr<gamestate::GameEntity> &entity,
	                                        const std::shared_ptr<openage::event::EventLoop> &loop,
	                                        const std::shared_ptr<openage::gamestate::GameState> &state,
	                                        const time::time_t &start_time);
};

} // namespace system
} // namespace gamestate
} // namespace openage
