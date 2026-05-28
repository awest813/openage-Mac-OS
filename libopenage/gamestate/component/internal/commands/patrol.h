// Copyright 2026-2026 the openage authors. See copying.md for legal info.

#pragma once

#include "coord/phys.h"
#include "gamestate/component/internal/commands/base_command.h"
#include "gamestate/component/internal/commands/types.h"


namespace openage::gamestate::component::command {

/**
 * Command for patrolling between two waypoints.
 *
 * The entity moves from \p waypoint_from toward \p waypoint_to; upon arrival
 * the waypoints are swapped so it returns to the starting position.
 */
class PatrolCommand : public Command {
public:
	/**
	 * Creates a new patrol command.
	 *
	 * @param waypoint_from Current position / start of the patrol segment.
	 * @param waypoint_to   Destination of the current patrol leg.
	 */
	PatrolCommand(coord::phys3 waypoint_from, coord::phys3 waypoint_to);
	virtual ~PatrolCommand() = default;

	inline command_t get_type() const override {
		return command_t::PATROL;
	}

	/**
	 * Get the starting waypoint (current patrol segment origin).
	 *
	 * @return Starting waypoint position.
	 */
	const coord::phys3 &get_waypoint_from() const;

	/**
	 * Get the destination waypoint (current patrol segment target).
	 *
	 * @return Destination waypoint position.
	 */
	const coord::phys3 &get_waypoint_to() const;

private:
	/**
	 * Start of the current patrol segment.
	 */
	coord::phys3 waypoint_from;

	/**
	 * Destination of the current patrol segment.
	 */
	coord::phys3 waypoint_to;
};

} // namespace openage::gamestate::component::command
