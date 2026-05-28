// Copyright 2026-2026 the openage authors. See copying.md for legal info.

#pragma once

#include <memory>

#include "event/eventhandler.h"
#include "time/time.h"


namespace openage {

namespace event {
class EventLoop;
class State;
}

namespace gamestate::event {

/**
 * Handles a "game.player_defeated" event.
 *
 * Fires when a player loses all their buildings.  Currently logs the event;
 * future work can use this to trigger a UI overlay or network broadcast.
 *
 * Params expected:
 *   - "player_id" (player_id_t) — the defeated player
 */
class PlayerDefeatedHandler : public openage::event::OnceEventHandler {
public:
	PlayerDefeatedHandler();
	~PlayerDefeatedHandler() = default;

	void setup_event(const std::shared_ptr<openage::event::Event> &event,
	                 const std::shared_ptr<openage::event::State> &state) override;

	void invoke(openage::event::EventLoop &loop,
	            const std::shared_ptr<openage::event::EventEntity> &target,
	            const std::shared_ptr<openage::event::State> &state,
	            const time::time_t &time,
	            const param_map &params) override;

	time::time_t predict_invoke_time(const std::shared_ptr<openage::event::EventEntity> &target,
	                                 const std::shared_ptr<openage::event::State> &state,
	                                 const time::time_t &at) override;
};

/**
 * Handles a "game.game_over" event.
 *
 * Fires when exactly one player remains alive.
 *
 * Params expected:
 *   - "winner_id" (player_id_t) — the winning player
 */
class GameOverHandler : public openage::event::OnceEventHandler {
public:
	GameOverHandler();
	~GameOverHandler() = default;

	void setup_event(const std::shared_ptr<openage::event::Event> &event,
	                 const std::shared_ptr<openage::event::State> &state) override;

	void invoke(openage::event::EventLoop &loop,
	            const std::shared_ptr<openage::event::EventEntity> &target,
	            const std::shared_ptr<openage::event::State> &state,
	            const time::time_t &time,
	            const param_map &params) override;

	time::time_t predict_invoke_time(const std::shared_ptr<openage::event::EventEntity> &target,
	                                 const std::shared_ptr<openage::event::State> &state,
	                                 const time::time_t &at) override;
};

} // namespace gamestate::event
} // namespace openage
