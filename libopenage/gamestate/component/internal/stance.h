// Copyright 2026-2026 the openage authors. See copying.md for legal info.

#pragma once

#include <memory>

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
 * Unit combat stance.
 */
enum class stance_t {
	AGGRESSIVE,   // attack any enemy in sight range
	DEFENSIVE,    // attack enemies that enter attack range
	STAND_GROUND, // attack without pursuing (no movement toward attacker)
	NO_ATTACK,    // never attack automatically
};

/**
 * Component that stores the current combat stance of a game entity over time.
 */
class Stance final : public InternalComponent {
public:
	/**
	 * Creates a Stance component.
	 *
	 * @param loop Event loop that all events from the component are registered on.
	 */
	Stance(const std::shared_ptr<openage::event::EventLoop> &loop);

	component_t get_type() const override;

	/**
	 * Get the stance curve (mutable).
	 *
	 * @return Reference to the stance curve.
	 */
	curve::Discrete<stance_t> &get_stances();

	/**
	 * Get the stance at a given time.
	 *
	 * @param time Time at which the stance is read.
	 *
	 * @return Stance value at \p time.
	 */
	stance_t get_stance(const time::time_t &time) const;

	/**
	 * Set the stance at a given time.
	 *
	 * @param time  Time at which the stance is set.
	 * @param stance New stance value.
	 */
	void set_stance(const time::time_t &time, stance_t stance);

private:
	/**
	 * Stance storage over time.
	 */
	curve::Discrete<stance_t> stance_curve;
};

} // namespace gamestate::component
} // namespace openage
