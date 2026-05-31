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

class Deconstruct {
public:
	static const time::time_t deconstruct_command(
		const std::shared_ptr<gamestate::GameEntity> &entity,
		const std::shared_ptr<openage::event::EventLoop> &loop,
		const std::shared_ptr<openage::gamestate::GameState> &state,
		const time::time_t &start_time);
};

} // namespace system
} // namespace gamestate
} // namespace openage
