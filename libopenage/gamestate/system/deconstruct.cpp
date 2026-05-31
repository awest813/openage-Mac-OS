// Copyright 2026-2026 the openage authors. See copying.md for legal info.

#include "deconstruct.h"

#include "event/event_loop.h"
#include "log/log.h"
#include "log/message.h"

#include "gamestate/component/internal/command_queue.h"
#include "gamestate/component/internal/commands/deconstruct.h"
#include "gamestate/component/internal/ownership.h"
#include "gamestate/component/internal/position.h"
#include "gamestate/component/types.h"
#include "gamestate/definitions.h"
#include "gamestate/game_entity.h"
#include "gamestate/game_state.h"
#include "gamestate/system/move.h"


namespace openage::gamestate::system {

static constexpr const char *DECONSTRUCT_COMPLETE_EVENT = "game.deconstruct_complete";
static constexpr double DECONSTRUCT_RANGE = gamestate::BUILDER_INTERACTION_RANGE;

const time::time_t Deconstruct::deconstruct_command(
    const std::shared_ptr<gamestate::GameEntity> &entity,
    const std::shared_ptr<openage::event::EventLoop> &loop,
    const std::shared_ptr<openage::gamestate::GameState> &state,
    const time::time_t &start_time) {
	auto command_queue = std::dynamic_pointer_cast<component::CommandQueue>(
		entity->get_component(component::component_t::COMMANDQUEUE));
	auto command = std::dynamic_pointer_cast<component::command::DeconstructCommand>(
		command_queue->pop_command(start_time));

	if (not command) [[unlikely]] {
		log::log(MSG(warn) << "Command is not a deconstruct command.");
		return time::time_t::from_int(0);
	}

	const auto target_id = command->get_target();
	const auto &entities = state->get_game_entities();
	auto target_it = entities.find(target_id);
	if (target_it == entities.end()) [[unlikely]] {
		log::log(MSG(warn) << "Deconstruct target " << target_id << " not found.");
		return time::time_t::from_int(0);
	}

	const auto &target = target_it->second;

	if (target->has_component(component::component_t::MOVE)) {
		log::log(MSG(warn) << "Entity " << target_id << " is not a building; cannot deconstruct.");
		return time::time_t::from_int(0);
	}

	auto building_cost = state->get_building_cost(target_id);
	if (not building_cost.has_value()) {
		log::log(MSG(warn) << "Entity " << target_id << " has no recorded cost; cannot deconstruct.");
		return time::time_t::from_int(0);
	}

	if (not entity->has_component(component::component_t::OWNERSHIP)
	    || not target->has_component(component::component_t::OWNERSHIP)) {
		log::log(MSG(warn) << "Entity " << entity->get_id()
		                   << " or deconstruct target " << target_id << " lacks ownership.");
		return time::time_t::from_int(0);
	}

	auto ownership = std::dynamic_pointer_cast<component::Ownership>(
		entity->get_component(component::component_t::OWNERSHIP));
	auto target_ownership = std::dynamic_pointer_cast<component::Ownership>(
		target->get_component(component::component_t::OWNERSHIP));
	auto owner_id = ownership->get_owners().get(start_time);
	if (target_ownership->get_owners().get(start_time) != owner_id) {
		log::log(MSG(dbg) << "Entity " << entity->get_id()
		                  << " cannot deconstruct building " << target_id << " owned by another player.");
		return time::time_t::from_int(0);
	}

	if (not target->has_component(component::component_t::POSITION)
	    || not entity->has_component(component::component_t::POSITION)) {
		return time::time_t::from_int(0);
	}

	auto target_pos_comp = std::dynamic_pointer_cast<component::Position>(
		target->get_component(component::component_t::POSITION));
	auto builder_pos_comp = std::dynamic_pointer_cast<component::Position>(
		entity->get_component(component::component_t::POSITION));
	auto target_pos = target_pos_comp->get_positions().get(start_time);
	auto builder_pos = builder_pos_comp->get_positions().get(start_time);

	if ((target_pos - builder_pos).length() > DECONSTRUCT_RANGE) {
		if (entity->has_component(component::component_t::MOVE)) {
			time::time_t move_time = Move::move_default(entity, state, target_pos, start_time);
			if (move_time > time::time_t::from_int(0)) {
				command_queue->add_command(
					start_time,
					std::make_shared<component::command::DeconstructCommand>(target_id));
				return move_time;
			}
		}
		return time::time_t::from_int(0);
	}

	auto cost_snapshot = building_cost.value();
	double deconstruct_duration = cost_snapshot.deconstruct_time;
	if (deconstruct_duration <= 0) {
		deconstruct_duration = DECONSTRUCT_TIME_FACTOR;
	}

	auto completion_time = start_time + deconstruct_duration;

	// Prevent destroy-salvage when the building is removed after deconstruction.
	state->clear_building_cost(target_id);

	openage::event::EventHandler::param_map params{
		{"building_id", target_id},
		{"building_pos", target_pos},
		{"cost_resource", cost_snapshot.resource_type},
		{"cost_amount", cost_snapshot.amount},
		{"recovery_fraction", cost_snapshot.deconstruct_recovery_fraction},
	};
	loop->create_event(DECONSTRUCT_COMPLETE_EVENT,
	                   entity->get_manager(),
	                   state,
	                   completion_time,
	                   params);

	log::log(MSG(info) << "Entity " << entity->get_id()
	                   << " started deconstructing building " << target_id
	                   << " (ready at t=" << completion_time << ").");

	return deconstruct_duration;
}

} // namespace openage::gamestate::system
