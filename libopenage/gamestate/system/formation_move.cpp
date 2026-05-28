// Copyright 2026-2026 the openage authors. See copying.md for legal info.

#include "formation_move.h"

#include "log/log.h"
#include "log/message.h"

#include "gamestate/component/internal/command_queue.h"
#include "gamestate/component/internal/commands/formation_move.h"
#include "gamestate/component/types.h"
#include "gamestate/game_entity.h"
#include "gamestate/game_state.h"
#include "gamestate/system/move.h"


namespace openage::gamestate::system {

const time::time_t FormationMove::formation_move_command(
	const std::shared_ptr<gamestate::GameEntity> &entity,
	const std::shared_ptr<openage::gamestate::GameState> &state,
	const time::time_t &start_time) {
	auto command_queue = std::dynamic_pointer_cast<component::CommandQueue>(
		entity->get_component(component::component_t::COMMANDQUEUE));
	auto command = std::dynamic_pointer_cast<component::command::FormationMoveCommand>(
		command_queue->pop_command(start_time));

	if (not command) [[unlikely]] {
		log::log(MSG(warn) << "Command is not a formation-move command.");
		return time::time_t::from_int(0);
	}

	// Resolve the per-unit destination: centroid + this unit's saved offset.
	auto destination = command->get_centroid() + command->get_offset();

	return Move::move_default(entity, state, destination, start_time);
}

} // namespace openage::gamestate::system
