// Copyright 2026-2026 the openage authors. See copying.md for legal info.

#pragma once

#include "coord/phys.h"
#include "gamestate/component/internal/commands/base_command.h"
#include "gamestate/component/internal/commands/types.h"


namespace openage::gamestate::component::command {

/**
 * Command to move a unit to a destination while preserving its offset from
 * the group's centroid (formation movement).
 *
 * The final destination for the unit is `centroid + offset`.
 */
class FormationMoveCommand final : public Command {
public:
	/**
	 * @param centroid Group destination (where the formation centre should end up).
	 * @param offset   This unit's displacement from the centroid at command issue time.
	 */
	FormationMoveCommand(const coord::phys3 &centroid,
	                     const coord::phys3_delta &offset) :
		centroid{centroid},
		offset{offset} {}

	command_t get_type() const override {
		return command_t::FORMATION_MOVE;
	}

	/**
	 * Get the formation centroid destination.
	 */
	const coord::phys3 &get_centroid() const {
		return this->centroid;
	}

	/**
	 * Get this unit's offset from the centroid.
	 */
	const coord::phys3_delta &get_offset() const {
		return this->offset;
	}

private:
	coord::phys3 centroid;
	coord::phys3_delta offset;
};

} // namespace openage::gamestate::component::command

