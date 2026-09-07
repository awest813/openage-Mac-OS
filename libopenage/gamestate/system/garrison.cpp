// Copyright 2026-2026 the openage authors. See copying.md for legal info.

#include "garrison.h"

#include "log/log.h"
#include "log/message.h"

#include "gamestate/component/internal/command_queue.h"
#include "gamestate/component/internal/commands/garrison.h"
#include "gamestate/component/internal/ownership.h"
#include "gamestate/component/internal/position.h"
#include "gamestate/component/types.h"
#include "gamestate/definitions.h"
#include "gamestate/game_entity.h"
#include "gamestate/game_state.h"
#include "gamestate/system/move.h"


namespace openage::gamestate::system {

const time::time_t Garrison::garrison_command(
    const std::shared_ptr<gamestate::GameEntity> &entity,
    const std::shared_ptr<openage::gamestate::GameState> &state,
    const time::time_t &start_time) {
	auto command_queue = std::dynamic_pointer_cast<component::CommandQueue>(
		entity->get_component(component::component_t::COMMANDQUEUE));
	auto command = std::dynamic_pointer_cast<component::command::GarrisonCommand>(
		command_queue->pop_command(start_time));

	if (not command) [[unlikely]] {
		log::log(MSG(warn) << "Command is not a garrison command.");
		return time::time_t::from_int(0);
	}

	const auto target_id = command->get_target();
	const auto &entities = state->get_game_entities();
	auto target_it = entities.find(target_id);
	if (target_it == entities.end()) [[unlikely]] {
		log::log(MSG(warn) << "Garrison target " << target_id << " not found.");
		return time::time_t::from_int(0);
	}

	const auto &target = target_it->second;

	if (not target->has_component(component::component_t::POSITION)
	    or not entity->has_component(component::component_t::POSITION)) {
		return time::time_t::from_int(0);
	}

	if (not state->can_garrison(entity->get_id(), target_id)) {
		log::log(MSG(dbg) << "Unit " << entity->get_id()
		                  << " cannot garrison in target " << target_id << ".");
		return time::time_t::from_int(0);
	}

	// Distance check: move closer if out of interaction range
	auto unit_pos_comp = std::dynamic_pointer_cast<component::Position>(
		entity->get_component(component::component_t::POSITION));
	auto target_pos_comp = std::dynamic_pointer_cast<component::Position>(
		target->get_component(component::component_t::POSITION));
	auto unit_pos = unit_pos_comp->get_positions().get(start_time);
	auto target_pos = target_pos_comp->get_positions().get(start_time);

	double dist = (target_pos - unit_pos).length();
	if (dist > GARRISON_INTERACTION_RANGE) {
		if (entity->has_component(component::component_t::MOVE)) {
			time::time_t move_time = Move::move_default(entity, state, target_pos, start_time);
			if (move_time > time::time_t::from_int(0)) {
				command_queue->add_command(
					start_time,
					std::make_shared<component::command::GarrisonCommand>(target_id));
				return move_time;
			}
		}
		return time::time_t::from_int(0);
	}

	// Within range: garrison the unit into target
	state->garrison_entity(entity->get_id(), target_id, start_time);
	return time::time_t::from_int(0);
}

const time::time_t Garrison::ungarrison_command(
    const std::shared_ptr<gamestate::GameEntity> &entity,
    const std::shared_ptr<openage::gamestate::GameState> &state,
    const time::time_t &start_time) {
	auto command_queue = std::dynamic_pointer_cast<component::CommandQueue>(
		entity->get_component(component::component_t::COMMANDQUEUE));
	if (command_queue and not command_queue->get_queue().empty(start_time)) {
		command_queue->pop_command(start_time);
	}

	state->ungarrison_entities(entity->get_id(), start_time);
	return time::time_t::from_int(0);
}

} // namespace openage::gamestate::system
