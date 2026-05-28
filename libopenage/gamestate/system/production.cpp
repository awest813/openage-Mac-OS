// Copyright 2026-2026 the openage authors. See copying.md for legal info.

#include "production.h"

#include <nyan/nyan.h>

#include "log/log.h"
#include "log/message.h"

#include "gamestate/component/api/create.h"
#include "gamestate/component/internal/command_queue.h"
#include "gamestate/component/internal/commands/train.h"
#include "gamestate/component/internal/ownership.h"
#include "gamestate/component/types.h"
#include "gamestate/game_entity.h"
#include "gamestate/game_state.h"
#include "gamestate/player.h"


namespace openage::gamestate::system {

const time::time_t Production::train_command(const std::shared_ptr<gamestate::GameEntity> &entity,
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
	auto &player = state->get_player(owner_id);
	auto db_view = player->get_db_view();

	// Retrieve nyan Create ability data
	auto create_component = std::dynamic_pointer_cast<component::Create>(
		entity->get_component(component::component_t::CREATE));
	auto create_ability = create_component->get_ability();

	const auto &target = command->get_game_entity();

	// Find the creatable whose game entity matches the requested unit.
	auto creatables = create_ability.get_set("Create.creatables");
	bool found = false;
	nyan::fqon_t cost_resource{};
	int64_t cost_amount = 0;
	double creation_time = 0.0;
	for (const auto &creatable_val : creatables) {
		auto creatable_fqon = std::dynamic_pointer_cast<nyan::ObjectValue>(
			creatable_val.get_ptr())->get_name();
		auto creatable_obj = db_view->get_object(creatable_fqon);

		auto game_entity = creatable_obj.get<nyan::ObjectValue>("CreatableGameEntity.game_entity");
		if (game_entity->get_name() != target) {
			continue;
		}

		creation_time = creatable_obj.get<nyan::Float>("CreatableGameEntity.creation_time")->get();
		cost_resource = creatable_obj.get<nyan::ObjectValue>("CreatableGameEntity.cost_resource")->get_name();
		cost_amount = creatable_obj.get<nyan::Int>("CreatableGameEntity.cost_amount")->get();
		found = true;
		break;
	}

	if (not found) [[unlikely]] {
		log::log(MSG(warn) << "Entity " << entity->get_id()
		                   << " cannot produce unknown game entity " << target << ".");
		return time::time_t::from_int(0);
	}

	// Resource cost check
	int64_t available = player->get_resource(start_time, cost_resource);
	if (available < cost_amount) {
		log::log(MSG(dbg) << "Player " << owner_id
		                  << " cannot afford " << target
		                  << " (cost=" << cost_amount << " of " << cost_resource
		                  << ", available=" << available << ").");
		return time::time_t::from_int(0);
	}

	// Deduct resources and queue the production request.
	player->add_resource(start_time, cost_resource, -cost_amount);
	auto completion_time = start_time + creation_time;
	state->request_production(owner_id, target, completion_time);

	log::log(MSG(info) << "Entity " << entity->get_id()
	                   << " started producing " << target
	                   << " for player " << owner_id
	                   << " (cost=" << cost_amount << " of " << cost_resource
	                   << ", ready at t=" << completion_time << ").");

	return creation_time;
}

} // namespace openage::gamestate::system
