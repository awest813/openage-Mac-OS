// Copyright 2021-2023 the openage authors. See copying.md for legal info.

#pragma once


namespace openage::gamestate::component {

/**
 * Types of components.
 */
enum class component_t {
	// Internal
	POSITION,
	COMMANDQUEUE,
	OWNERSHIP,
	ACTIVITY,
	STANCE,

	// API
	IDLE,
	TURN,
	MOVE,
	ATTACK,
	GATHER,
	CREATE,
	SELECTABLE,
	LIVE
};

} // namespace openage::gamestate::component
