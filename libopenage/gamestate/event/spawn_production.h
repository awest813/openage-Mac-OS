// Copyright 2026-2026 the openage authors. See copying.md for legal info.

#pragma once

#include <memory>

#include "event/eventhandler.h"
#include "time/time.h"


namespace openage {

namespace event {
class EventLoop;
}

namespace gamestate {

class EntityFactory;

namespace event {

/**
 * Handles spawning a unit that has finished training.
 *
 * Fired by the Production system at the exact moment a unit's
 * creation_time expires. Params expected:
 *   - "owner"       (player_id_t) — owner of the new unit
 *   - "game_entity" (std::string) — nyan fqon of the unit to spawn
 *
 * The unit is placed at the producing building's position (or world
 * origin if the position is unavailable).
 */
class SpawnProductionHandler : public openage::event::OnceEventHandler {
public:
	SpawnProductionHandler(const std::shared_ptr<openage::event::EventLoop> &loop,
	                       const std::shared_ptr<gamestate::EntityFactory> &factory);
	~SpawnProductionHandler() = default;

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

private:
	std::shared_ptr<openage::event::EventLoop> loop;
	std::shared_ptr<gamestate::EntityFactory> factory;
};

} // namespace event
} // namespace gamestate
} // namespace openage
