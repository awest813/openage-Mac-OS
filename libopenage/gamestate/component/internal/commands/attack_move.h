// Copyright 2026-2026 the openage authors. See copying.md for legal info.

#pragma once

#include "coord/phys.h"
#include "gamestate/component/internal/commands/base_command.h"
#include "gamestate/component/internal/commands/types.h"


namespace openage::gamestate::component::command {

/**
 * Command for moving toward a destination while attacking enemies along the way.
 */
class AttackMoveCommand : public Command {
public:
	/**
	 * Creates a new attack-move command.
	 *
	 * @param destination Target position to move toward.
	 */
	AttackMoveCommand(coord::phys3 destination);
	virtual ~AttackMoveCommand() = default;

	inline command_t get_type() const override {
		return command_t::ATTACK_MOVE;
	}

	/**
	 * Get the destination coordinates.
	 *
	 * @return Destination position.
	 */
	const coord::phys3 &get_destination() const;

private:
	/**
	 * Target position to move toward.
	 */
	coord::phys3 destination;
};

} // namespace openage::gamestate::component::command
