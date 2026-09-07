// Copyright 2026-2026 the openage authors. See copying.md for legal info.

#pragma once

#include "gamestate/component/internal/commands/base_command.h"
#include "gamestate/component/internal/commands/types.h"
#include "gamestate/types.h"


namespace openage::gamestate::component::command {

/**
 * Command for researching a technology at a building.
 */
class ResearchCommand : public Command {
public:
	/**
	 * Creates a new research command.
	 *
	 * @param tech_id ID of the technology to research.
	 */
	ResearchCommand(int64_t tech_id);
	virtual ~ResearchCommand() = default;

	inline command_t get_type() const override {
		return command_t::RESEARCH;
	}

	/**
	 * Get the technology ID to research.
	 */
	int64_t get_tech_id() const;

private:
	int64_t tech_id;
};

} // namespace openage::gamestate::component::command
