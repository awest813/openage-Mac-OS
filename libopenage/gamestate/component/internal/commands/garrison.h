// Copyright 2026-2026 the openage authors. See copying.md for legal info.

#pragma once

#include "gamestate/component/internal/commands/base_command.h"
#include "gamestate/component/internal/commands/types.h"
#include "gamestate/types.h"


namespace openage::gamestate::component::command {

/**
 * Command for a unit to enter a garrisonable building or vehicle.
 */
class GarrisonCommand : public Command {
public:
	explicit GarrisonCommand(entity_id_t target_entity);
	~GarrisonCommand() override = default;

	command_t get_type() const override {
		return command_t::GARRISON;
	}

	entity_id_t get_target() const;

private:
	entity_id_t target_entity;
};

/**
 * Command for a building or transport to eject garrisoned units.
 */
class UngarrisonCommand : public Command {
public:
	UngarrisonCommand() = default;
	~UngarrisonCommand() override = default;

	command_t get_type() const override {
		return command_t::UNGARRISON;
	}
};

} // namespace openage::gamestate::component::command
