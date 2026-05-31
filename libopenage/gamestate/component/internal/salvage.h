// Copyright 2026-2026 the openage authors. See copying.md for legal info.

#pragma once

#include <memory>
#include <string>

#include "curve/discrete.h"
#include "gamestate/component/internal_component.h"
#include "gamestate/component/types.h"
#include "time/time.h"


namespace openage {
namespace event {
class EventLoop;
}

namespace gamestate::component {

/**
 * Salvage left behind when a building is destroyed.
 *
 * Neutral gather target: villagers harvest the remaining amount and receive
 * the stored resource type. Amount decays over time via GameState::tick_salvage_decay.
 */
class Salvage final : public InternalComponent {
public:
	Salvage(const std::shared_ptr<openage::event::EventLoop> &loop,
	        const std::string &resource_type,
	        int64_t starting_amount,
	        const time::time_t &creation_time);

	component_t get_type() const override;

	const std::string &get_resource_type() const;

	int64_t get_amount(const time::time_t &time) const;

	void set_amount(const time::time_t &time, int64_t amount);

	time::time_t get_last_decay_time() const;

	void set_last_decay_time(const time::time_t &time);

private:
	std::string resource_type;
	curve::Discrete<int64_t> amount_curve;
	time::time_t last_decay_time;
};

} // namespace gamestate::component
} // namespace openage
