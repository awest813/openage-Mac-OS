// Copyright 2024-2024 the openage authors. See copying.md for legal info.

#pragma once

#include "gamestate/component/internal/commands/base_command.h"
#include "gamestate/component/internal/commands/types.h"
#include "gamestate/types.h"


namespace openage::gamestate::component::command {

/**
 * Command for attacking a target entity.
 */
class AttackCommand : public Command {
public:
	/**
	 * Creates a new attack command.
	 *
	 * @param target ID of the entity to attack.
	 */
	AttackCommand(entity_id_t target);
	virtual ~AttackCommand() = default;

	inline command_t get_type() const override {
		return command_t::ATTACK;
	}

	/**
	 * Get the target entity ID.
	 *
	 * @return Target entity ID.
	 */
	entity_id_t get_target() const;

private:
	/**
	 * ID of the entity to attack.
	 */
	entity_id_t target;
};

} // namespace openage::gamestate::component::command
