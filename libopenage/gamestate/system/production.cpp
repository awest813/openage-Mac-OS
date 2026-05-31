// Copyright 2026-2026 the openage authors. See copying.md for legal info.

#include "production.h"

#include <nyan/nyan.h>

#include "event/event_loop.h"
#include "log/log.h"
#include "log/message.h"

#include "gamestate/api/creatable.h"
#include "gamestate/component/api/create.h"
#include "gamestate/component/internal/command_queue.h"
#include "gamestate/component/internal/commands/train.h"
#include "gamestate/component/internal/ownership.h"
#include "gamestate/component/types.h"
#include "gamestate/definitions.h"
#include "gamestate/game_entity.h"
#include "gamestate/game_state.h"
#include "gamestate/manager.h"
#include "gamestate/player.h"


namespace openage::gamestate::system {

static constexpr const char *SPAWN_PRODUCTION_EVENT = "game.spawn_production";

const time::time_t Production::train_command(const std::shared_ptr<gamestate::GameEntity> &entity,
                                             const std::shared_ptr<openage::event::EventLoop> &loop,
                                             const std::shared_ptr<openage::gamestate::GameState> &state,
                                             const time::time_t &start_time) {
	auto command_queue = std::dynamic_pointer_cast<component::CommandQueue>(
		entity->get_component(component::component_t::COMMANDQUEUE));
	auto command = std::dynamic_pointer_cast<component::command::TrainCommand>(
		command_queue->pop_command(start_time));

	if (not command) [[unlikely]] {
		log::log(MSG(warn) << "Command is not a train command.");
		return time::time_t::from_int(0);
	}

	if (not entity->has_component(component::component_t::CREATE)) [[unlikely]] {
		log::log(WARN << "Entity " << entity->get_id() << " has no create component.");
		return time::time_t::from_int(0);
	}

	if (not entity->has_component(component::component_t::OWNERSHIP)) [[unlikely]] {
		log::log(WARN << "Entity " << entity->get_id() << " has no ownership component.");
		return time::time_t::from_int(0);
	}

	auto ownership = std::dynamic_pointer_cast<component::Ownership>(
		entity->get_component(component::component_t::OWNERSHIP));
	auto owner_id = ownership->get_owners().get(start_time);
	if (not state->has_player(owner_id)) [[unlikely]] {
		log::log(MSG(warn) << "Entity " << entity->get_id()
		                   << " has unknown owner " << owner_id << "; cannot train.");
		return time::time_t::from_int(0);
	}
	auto &player = state->get_player(owner_id);
	auto db_view = player->get_db_view();

	auto create_component = std::dynamic_pointer_cast<component::Create>(
		entity->get_component(component::component_t::CREATE));
	auto create_ability = create_component->get_ability();

	const auto &target = command->get_game_entity();
	auto creatable = api::lookup_creatable(db_view, create_ability, target);

	if (not creatable.found) [[unlikely]] {
		log::log(MSG(warn) << "Entity " << entity->get_id()
		                   << " cannot produce unknown game entity " << target << ".");
		return time::time_t::from_int(0);
	}

	auto cost_record = api::building_cost_from_creatable(creatable);

	int64_t available = player->get_resource(start_time, nyan::fqon_t{cost_record.resource_type});
	if (available < cost_record.amount) {
		log::log(MSG(dbg) << "Player " << owner_id
		                  << " cannot afford " << target
		                  << " (cost=" << cost_record.amount << " of " << cost_record.resource_type
		                  << ", available=" << available << ").");
		return time::time_t::from_int(0);
	}

	if (not player->has_population_space(start_time, DEFAULT_POPULATION_COST)) {
		log::log(MSG(dbg) << "Player " << owner_id
		                  << " cannot train " << target
		                  << ": population cap reached (demand="
		                  << player->get_population_demand(start_time)
		                  << ", capacity=" << player->get_population_capacity(start_time) << ").");
		return time::time_t::from_int(0);
	}

	player->add_resource(start_time, nyan::fqon_t{cost_record.resource_type}, -cost_record.amount);
	player->add_population_demand(start_time, DEFAULT_POPULATION_COST);

	auto completion_time = start_time + creatable.creation_time;

	state->request_production(owner_id, target, completion_time);

	// Pass construction cost for buildings trained from a town center (and any
	// other Create producer) so salvage/deconstruct use the correct fractions.
	openage::event::EventHandler::param_map params{
		{"owner", owner_id},
		{"game_entity", target},
		{"build_cost_resource", cost_record.resource_type},
		{"build_cost_amount", cost_record.amount},
		{"salvage_recovery_fraction", cost_record.destroy_recovery_fraction},
		{"deconstruct_recovery_fraction", cost_record.deconstruct_recovery_fraction},
		{"deconstruct_time", cost_record.deconstruct_time},
	};
	loop->create_event(SPAWN_PRODUCTION_EVENT,
	                   entity->get_manager(),
	                   state,
	                   completion_time,
	                   params);

	log::log(MSG(info) << "Entity " << entity->get_id()
	                   << " started producing " << target
	                   << " for player " << owner_id
	                   << " (cost=" << cost_record.amount << " of " << cost_record.resource_type
	                   << ", ready at t=" << completion_time << ").");

	return creatable.creation_time;
}

} // namespace openage::gamestate::system
