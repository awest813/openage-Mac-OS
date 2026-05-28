// Copyright 2026-2026 the openage authors. See copying.md for legal info.

#pragma once

#include <string>

#include "coord/phys.h"
#include "gamestate/component/internal/commands/base_command.h"
#include "gamestate/component/internal/commands/types.h"


namespace openage::gamestate::component::command {

/**
 * Command for placing a new building at a target position.
 *
 * A villager (or other builder entity) receives this command to construct
 * a building.  The builder walks to the target position and then begins
 * construction; when complete the building entity is spawned.
 */
class BuildCommand : public Command {
public:
	/**
	 * Creates a new build command.
	 *
	 * @param building nyan fqon of the building to construct.
	 * @param target   World position where the building should be placed.
	 */
	BuildCommand(const std::string &building, const coord::phys3 &target);
	virtual ~BuildCommand() = default;

	inline command_t get_type() const override {
		return command_t::BUILD;
	}

	/**
	 * Get the nyan fqon of the building to construct.
	 *
	 * @return Building identifier (nyan fqon).
	 */
	const std::string &get_building() const;

	/**
	 * Get the target position for the building.
	 *
	 * @return World coordinates of the build site.
	 */
	const coord::phys3 &get_target() const;

private:
	/**
	 * nyan fqon of the building to construct.
	 */
	std::string building;

	/**
	 * World position of the build site.
	 */
	coord::phys3 target;
};

} // namespace openage::gamestate::component::command
