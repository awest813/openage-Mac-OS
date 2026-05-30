// Copyright 2026-2026 the openage authors. See copying.md for legal info.

#include "spawn_production.h"

#include <string>

#include "coord/phys.h"
#include "gamestate/component/internal/activity.h"
#include "gamestate/component/internal/command_queue.h"
#include "gamestate/component/internal/commands/move.h"
#include "gamestate/component/internal/ownership.h"
#include "gamestate/component/internal/position.h"
#include "gamestate/component/types.h"
#include "gamestate/definitions.h"
#include "gamestate/entity_factory.h"
#include "gamestate/game_entity.h"
#include "gamestate/game_state.h"
#include "gamestate/manager.h"
#include "gamestate/map.h"
#include "gamestate/player.h"
#include "gamestate/types.h"
#include "log/log.h"
#include "log/message.h"


namespace openage::gamestate::event {

SpawnProductionHandler::SpawnProductionHandler(const std::shared_ptr<openage::event::EventLoop> &loop,
                                               const std::shared_ptr<gamestate::EntityFactory> &factory) :
	OnceEventHandler{"game.spawn_production"},
	loop{loop},
	factory{factory} {
}

void SpawnProductionHandler::setup_event(const std::shared_ptr<openage::event::Event> & /* event */,
                                         const std::shared_ptr<openage::event::State> & /* state */) {
	// nothing
}

void SpawnProductionHandler::invoke(openage::event::EventLoop & /* loop */,
                                    const std::shared_ptr<openage::event::EventEntity> &target,
                                    const std::shared_ptr<openage::event::State> &state,
                                    const time::time_t &time,
                                    const param_map &params) {
	auto gstate = std::dynamic_pointer_cast<gamestate::GameState>(state);

	auto owner_id = params.get("owner", player_id_t{0});
	auto nyan_entity = params.get("game_entity", std::string{});

	if (nyan_entity.empty()) [[unlikely]] {
		log::log(MSG(warn) << "SpawnProductionHandler: no game_entity param.");
		return;
	}

	// Drain finished entries from the production queue so it stays bounded and
	// reflects only units still in production. Each finished unit has its own
	// spawn event, so the actual spawn below is driven by the event params, not
	// the drained list.
	gstate->take_completed_productions(time);

	// BUILD requests carry an explicit placement position (where the player
	// chose to put the building). TRAIN requests do not, and instead derive
	// the spawn position from the producing building below.
	coord::phys3 spawn_pos = gamestate::WORLD_ORIGIN;
	bool has_explicit_pos = params.check_type<coord::phys3>("spawn_pos");
	if (has_explicit_pos) {
		spawn_pos = params.get("spawn_pos", gamestate::WORLD_ORIGIN);
	}
	else if (target) {
		// target is the producing building's GameEntityManager; its id() is the entity id.
		auto entity_id = static_cast<gamestate::entity_id_t>(target->id());
		const auto &entities = gstate->get_game_entities();
		auto it = entities.find(entity_id);
		if (it != entities.end()) {
			auto &producer = it->second;
			if (producer->has_component(component::component_t::POSITION)) {
				auto pos_comp = std::dynamic_pointer_cast<component::Position>(
					producer->get_component(component::component_t::POSITION));
				spawn_pos = pos_comp->get_positions().get(time);
				// Offset one tile so the new unit doesn't overlap the building.
				spawn_pos.ne += 1;
			}
		}
	}

	// Validate the spawn position is within the map.
	if (gstate->get_map()) {
		auto map_size = gstate->get_map()->get_size();
		if (not(spawn_pos.ne >= 0
		        and spawn_pos.ne < map_size[0]
		        and spawn_pos.se >= 0
		        and spawn_pos.se < map_size[1])) {
			spawn_pos = gamestate::WORLD_ORIGIN;
		}
	}

	// Create the new entity.
	auto entity = this->factory->add_game_entity(this->loop, gstate, owner_id, nyan_entity);

	auto entity_pos = std::dynamic_pointer_cast<component::Position>(
		entity->get_component(component::component_t::POSITION));
	entity_pos->set_position(time, spawn_pos);
	entity_pos->set_angle(time, coord::phys_angle_t::from_int(315));

	auto entity_owner = std::dynamic_pointer_cast<component::Ownership>(
		entity->get_component(component::component_t::OWNERSHIP));
	entity_owner->set_owner(time, owner_id);

	// If the producing entity has a rally point set, send the new unit there.
	// Queue the move before the activity is initialised so it is processed on
	// the first activity pass. Only mobile units with a command queue qualify.
	if (target
	    and entity->has_component(component::component_t::MOVE)
	    and entity->has_component(component::component_t::COMMANDQUEUE)) {
		auto producer_id = static_cast<gamestate::entity_id_t>(target->id());
		auto rally_point = gstate->get_rally_point(producer_id);
		if (rally_point.has_value()) {
			auto command_queue = std::dynamic_pointer_cast<component::CommandQueue>(
				entity->get_component(component::component_t::COMMANDQUEUE));
			command_queue->add_command(
				time,
				std::make_shared<component::command::MoveCommand>(rally_point.value()));
		}
	}

	// A completed building raises its owner's population capacity. Buildings are
	// identified by the same heuristic used elsewhere (owned, no MOVE component).
	if (not entity->has_component(component::component_t::MOVE)
	    and gstate->has_player(owner_id)) {
		gstate->get_player(owner_id)->add_population_capacity(
			time, gamestate::DEFAULT_BUILDING_POPULATION_SPACE);
	}

	auto activity = std::dynamic_pointer_cast<component::Activity>(
		entity->get_component(component::component_t::ACTIVITY));
	activity->init(time);
	entity->get_manager()->run_activity_system(time);

	gstate->add_game_entity(entity);

	log::log(MSG(info) << "Spawned produced unit " << nyan_entity
	                   << " (id=" << entity->get_id()
	                   << ") for player " << owner_id
	                   << " at " << spawn_pos << ".");
}

time::time_t SpawnProductionHandler::predict_invoke_time(const std::shared_ptr<openage::event::EventEntity> & /* target */,
                                                          const std::shared_ptr<openage::event::State> & /* state */,
                                                          const time::time_t &at) {
	return at;
}

} // namespace openage::gamestate::event
