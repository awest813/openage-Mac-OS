// Copyright 2026-2026 the openage authors. See copying.md for legal info.

#pragma once

#include "gamestate/component/internal/commands/base_command.h"
#include "gamestate/component/internal/commands/types.h"
#include "gamestate/types.h"


namespace openage::gamestate::component::command {

/**
 * Command for guarding (following and protecting) another entity.
 */
class GuardCommand : public Command {
public:
	/**
	 * Creates a new guard command.
	 *
	 * @param target_id ID of the entity to guard.
	 */
	GuardCommand(entity_id_t target_id);
	virtual ~GuardCommand() = default;

	inline command_t get_type() const override {
		return command_t::GUARD;
	}

	/**
	 * Get the ID of the entity being guarded.
	 *
	 * @return Target entity ID.
	 */
	entity_id_t get_target_id() const;

private:
	/**
	 * ID of the entity to guard.
	 */
	entity_id_t target_id;
};

} // namespace openage::gamestate::component::command
