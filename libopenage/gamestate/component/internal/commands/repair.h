// Copyright 2026-2026 the openage authors. See copying.md for legal info.

#pragma once

#include "gamestate/component/internal/commands/base_command.h"
#include "gamestate/component/internal/commands/types.h"
#include "gamestate/types.h"


namespace openage::gamestate::component::command {

/**
 * Command for repairing a damaged building or unit.
 *
 * The repairer approaches the target, validates ownership and damage, and
 * restores HP over time while deducting proportional resources.
 */
class RepairCommand : public Command {
public:
	explicit RepairCommand(entity_id_t target_entity);
	~RepairCommand() override = default;

	command_t get_type() const override {
		return command_t::REPAIR;
	}

	entity_id_t get_target() const;

private:
	entity_id_t target_entity;
};

} // namespace openage::gamestate::component::command
