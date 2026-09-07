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
 * System for trade units (Trade Carts and Trade Cogs) running trade routes.
 */
class Trade {
public:
	/**
	 * Executes a trade command for an entity.
	 *
	 * Navigates between target market and home market, loads gold cargo at the
	 * target market, delivers it to the home market, updates player trade stats,
	 * and loops indefinitely until cancelled or markets are destroyed.
	 *
	 * @param entity     Trade cart / cog running the route.
	 * @param state      Current game state.
	 * @param start_time Current simulation time.
	 * @return Duration of this step (movement time or 0).
	 */
	static const time::time_t trade_command(
	    const std::shared_ptr<gamestate::GameEntity> &entity,
	    const std::shared_ptr<openage::gamestate::GameState> &state,
	    const time::time_t &start_time);
};

} // namespace system
} // namespace gamestate
} // namespace openage
