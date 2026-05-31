// Copyright 2026-2026 the openage authors. See copying.md for legal info.

#include "build.h"

#include <nyan/nyan.h>

#include "event/event_loop.h"
#include "log/log.h"
#include "log/message.h"

#include "gamestate/api/creatable.h"
#include "gamestate/component/api/create.h"
#include "gamestate/component/internal/command_queue.h"
#include "gamestate/component/internal/commands/build.h"
#include "gamestate/component/internal/ownership.h"
#include "gamestate/component/internal/position.h"
#include "gamestate/component/types.h"
#include "gamestate/game_entity.h"
#include "gamestate/game_state.h"
#include "gamestate/manager.h"
#include "gamestate/player.h"
#include "gamestate/system/move.h"


namespace openage::gamestate::system {

static constexpr const char *SPAWN_PRODUCTION_EVENT = "game.spawn_production";

/**
 * Maximum distance (in tiles) between the builder and the build site at which
 * construction may begin. Farther than this, the builder walks toward the site.
 */
static constexpr double BUILD_RANGE = 2.0;

const time::time_t Build::build_command(const std::shared_ptr<gamestate::GameEntity> &entity,
                                        const std::shared_ptr<openage::event::EventLoop> &loop,
                                        const std::shared_ptr<openage::gamestate::GameState> &state,
                                        const time::time_t &start_time) {
	auto command_queue = std::dynamic_pointer_cast<component::CommandQueue>(
		entity->get_component(component::component_t::COMMANDQUEUE));
	auto command = std::dynamic_pointer_cast<component::command::BuildCommand>(
		command_queue->pop_command(start_time));

	if (not command) [[unlikely]] {
		log::log(MSG(warn) << "Command is not a build command.");
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

	const auto &target_building = command->get_building();
	const coord::phys3 &build_site = command->get_target();

	// Walk to the build site before construction can begin. If the builder can
	// move and is still farther than BUILD_RANGE from the site, step toward it
	// and re-enqueue the build command so it is retried on arrival (mirrors the
	// AttackMove / Patrol self-re-enqueue pattern). Construction only starts —
	// and resources are only spent — once the builder has reached the site.
	if (entity->has_component(component::component_t::POSITION)
	    and entity->has_component(component::component_t::MOVE)) {
		auto pos_comp = std::dynamic_pointer_cast<component::Position>(
			entity->get_component(component::component_t::POSITION));
		auto current_pos = pos_comp->get_positions().get(start_time);
		double dist = (build_site - current_pos).length();
		if (dist > BUILD_RANGE) {
			time::time_t move_time = Move::move_default(entity, state, build_site, start_time);
			if (move_time > time::time_t::from_int(0)) {
				command_queue->add_command(
					start_time,
					std::make_shared<component::command::BuildCommand>(target_building, build_site));
				return move_time;
			}
		}
	}

	auto ownership = std::dynamic_pointer_cast<component::Ownership>(
		entity->get_component(component::component_t::OWNERSHIP));
	auto owner_id = ownership->get_owners().get(start_time);
	if (not state->has_player(owner_id)) [[unlikely]] {
		log::log(MSG(warn) << "Entity " << entity->get_id()
		                   << " has unknown owner " << owner_id << "; cannot build.");
		return time::time_t::from_int(0);
	}
	auto &player = state->get_player(owner_id);
	auto db_view = player->get_db_view();

	auto create_component = std::dynamic_pointer_cast<component::Create>(
		entity->get_component(component::component_t::CREATE));
	auto create_ability = create_component->get_ability();

	auto creatable = api::lookup_creatable(db_view, create_ability, target_building);
	if (not creatable.found) [[unlikely]] {
		log::log(MSG(warn) << "Entity " << entity->get_id()
		                   << " cannot build unknown building " << target_building << ".");
		return time::time_t::from_int(0);
	}

	auto cost_record = api::building_cost_from_creatable(creatable);

	int64_t available = player->get_resource(start_time, nyan::fqon_t{cost_record.resource_type});
	if (available < cost_record.amount) {
		log::log(MSG(dbg) << "Player " << owner_id
		                  << " cannot afford " << target_building
		                  << " (cost=" << cost_record.amount << " of " << cost_record.resource_type
		                  << ", available=" << available << ").");
		return time::time_t::from_int(0);
	}

	player->add_resource(start_time, nyan::fqon_t{cost_record.resource_type}, -cost_record.amount);

	auto completion_time = start_time + creatable.creation_time;

	state->request_production(owner_id, target_building, completion_time);

	openage::event::EventHandler::param_map params{
		{"owner", owner_id},
		{"game_entity", target_building},
		{"spawn_pos", build_site},
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
	                   << " started building " << target_building
	                   << " for player " << owner_id
	                   << " (cost=" << cost_record.amount << " of " << cost_record.resource_type
	                   << ", at " << build_site
	                   << ", ready at t=" << completion_time << ").");

	return creatable.creation_time;
}

} // namespace openage::gamestate::system
