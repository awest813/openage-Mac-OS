// Copyright 2023-2023 the openage authors. See copying.md for legal info.

#pragma once


namespace openage::gamestate::component::command {

/**
 * Command types.
 */
enum class command_t {
	NONE,

	CUSTOM,
	IDLE,
	MOVE,
	ATTACK,
	ATTACK_MOVE,
	GATHER,
	TRAIN,
	PATROL,
	GUARD,
	SET_STANCE,
	FORMATION_MOVE,
};

} // namespace openage::gamestate::component::command
