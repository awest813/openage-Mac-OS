// Copyright 2026-2026 the openage authors. See copying.md for legal info.

#pragma once

#include "gamestate/component/internal/commands/base_command.h"
#include "gamestate/component/internal/commands/types.h"
#include "gamestate/types.h"


namespace openage::gamestate::component::command {

/**
 * Command for deconstructing an existing building to recover materials.
 *
 * The builder walks to the building, waits for deconstruct_time, then salvage
 * is spawned and the building is removed.
 */
class DeconstructCommand : public Command {
public:
	explicit DeconstructCommand(entity_id_t target_building);
	~DeconstructCommand() override = default;

	command_t get_type() const override {
		return command_t::DECONSTRUCT;
	}

	entity_id_t get_target() const;

private:
	entity_id_t target_building;
};

} // namespace openage::gamestate::component::command
