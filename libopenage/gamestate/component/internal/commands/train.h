// Copyright 2026-2026 the openage authors. See copying.md for legal info.

#pragma once

#include <string>

#include "gamestate/component/internal/commands/base_command.h"
#include "gamestate/component/internal/commands/types.h"
#include "gamestate/types.h"


namespace openage::gamestate::component::command {

/**
 * Command for training (producing) a new unit from a building.
 */
class TrainCommand : public Command {
public:
	/**
	 * Creates a new train command.
	 *
	 * @param game_entity nyan fqon of the game entity (unit) to produce.
	 */
	TrainCommand(const std::string &game_entity);
	virtual ~TrainCommand() = default;

	inline command_t get_type() const override {
		return command_t::TRAIN;
	}

	/**
	 * Get the nyan fqon of the game entity to produce.
	 *
	 * @return Game entity identifier (nyan fqon).
	 */
	const std::string &get_game_entity() const;

private:
	/**
	 * nyan fqon of the game entity (unit) to produce.
	 */
	std::string game_entity;
};

} // namespace openage::gamestate::component::command
