// Copyright 2024-2024 the openage authors. See copying.md for legal info.

#pragma once

#include "gamestate/component/internal/commands/base_command.h"
#include "gamestate/component/internal/commands/types.h"
#include "gamestate/types.h"


namespace openage::gamestate::component::command {

/**
 * Command for gathering resources from a target entity.
 */
class GatherCommand : public Command {
public:
	/**
	 * Creates a new gather command.
	 *
	 * @param target ID of the resource entity to gather from.
	 */
	GatherCommand(entity_id_t target);
	virtual ~GatherCommand() = default;

	inline command_t get_type() const override {
		return command_t::GATHER;
	}

	/**
	 * Get the target resource entity ID.
	 *
	 * @return Target entity ID.
	 */
	entity_id_t get_target() const;

private:
	/**
	 * ID of the resource entity to gather from.
	 */
	entity_id_t target;
};

} // namespace openage::gamestate::component::command
