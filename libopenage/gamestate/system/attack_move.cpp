// Copyright 2026-2026 the openage authors. See copying.md for legal info.

#include "attack_move.h"

#include <nyan/nyan.h>

#include "log/log.h"
#include "log/message.h"

#include "gamestate/component/api/attack.h"
#include "gamestate/component/api/live.h"
#include "gamestate/component/internal/command_queue.h"
#include "gamestate/component/internal/commands/attack.h"
#include "gamestate/component/internal/commands/attack_move.h"
#include "gamestate/component/internal/ownership.h"
#include "gamestate/component/internal/position.h"
#include "gamestate/component/types.h"
#include "gamestate/game_entity.h"
#include "gamestate/game_state.h"
#include "gamestate/system/move.h"
#include "gamestate/types.h"


namespace openage::gamestate::system {

const time::time_t AttackMove::attack_move_command(
	const std::shared_ptr<gamestate::GameEntity> &entity,
	const std::shared_ptr<openage::gamestate::GameState> &state,
	const time::time_t &start_time) {
	// Pop the ATTACK_MOVE command from the queue
	auto command_queue = std::dynamic_pointer_cast<component::CommandQueue>(
		entity->get_component(component::component_t::COMMANDQUEUE));
	auto command = std::dynamic_pointer_cast<component::command::AttackMoveCommand>(
		command_queue->pop_command(start_time));

	if (not command) [[unlikely]] {
		log::log(MSG(warn) << "Command is not an attack-move command.");
		return time::time_t::from_int(0);
	}

	const coord::phys3 &destination = command->get_destination();

	// Check if entity has a position
	if (not entity->has_component(component::component_t::POSITION)) {
		return time::time_t::from_int(0);
	}

	auto pos_comp = std::dynamic_pointer_cast<component::Position>(
		entity->get_component(component::component_t::POSITION));
	auto current_pos = pos_comp->get_positions().get(start_time);

	// Check if we have arrived at the destination
	auto delta = destination - current_pos;
	double dist = delta.length();
	if (dist < 0.5) {
		// Arrived — done
		return time::time_t::from_int(0);
	}

	// Get own owner for enemy identification
	player_id_t own_owner = 0;
	bool has_ownership = entity->has_component(component::component_t::OWNERSHIP);
	if (has_ownership) {
		auto own_owner_comp = std::dynamic_pointer_cast<component::Ownership>(
			entity->get_component(component::component_t::OWNERSHIP));
		own_owner = own_owner_comp->get_owners().get(start_time);
	}

	// Determine attack range if possible
	double max_range = 0.0;
	bool has_attack = entity->has_component(component::component_t::ATTACK);
	if (has_attack) {
		auto attack_comp = std::dynamic_pointer_cast<component::Attack>(
			entity->get_component(component::component_t::ATTACK));
		auto attack_ability = attack_comp->get_ability();
		auto range_val = attack_ability.get<nyan::Float>("Attack.max_range");
		max_range = range_val->get();
	}

	// Scan all entities for enemies within attack range
	entity_id_t found_enemy = 0;
	bool enemy_found = false;

	if (has_attack && has_ownership) {
		const auto &entities = state->get_game_entities();
		for (const auto &[candidate_id, candidate] : entities) {
			if (candidate_id == entity->get_id()) {
				continue;
			}
			if (not candidate->has_component(component::component_t::POSITION)) {
				continue;
			}
			if (not candidate->has_component(component::component_t::LIVE)) {
				continue;
			}
			if (not candidate->has_component(component::component_t::OWNERSHIP)) {
				continue;
			}

			auto candidate_owner_comp = std::dynamic_pointer_cast<component::Ownership>(
				candidate->get_component(component::component_t::OWNERSHIP));
			player_id_t candidate_owner = candidate_owner_comp->get_owners().get(start_time);

			if (candidate_owner == own_owner) {
				continue; // same team, skip
			}

			auto candidate_pos_comp = std::dynamic_pointer_cast<component::Position>(
				candidate->get_component(component::component_t::POSITION));
			auto candidate_pos = candidate_pos_comp->get_positions().get(start_time);
			auto to_candidate = candidate_pos - current_pos;
			double candidate_dist = to_candidate.length();

			if (candidate_dist <= max_range) {
				found_enemy = candidate_id;
				enemy_found = true;
				break;
			}
		}
	}

	if (enemy_found) {
		// Queue an attack first, then re-queue the attack-move to continue afterward.
		command_queue->add_command(start_time,
		                           std::make_shared<component::command::AttackCommand>(found_enemy));
		command_queue->add_command(start_time,
		                           std::make_shared<component::command::AttackMoveCommand>(destination));
		return time::time_t::from_int(0);
	}

	// No enemy found — move toward destination
	time::time_t move_time = Move::move_default(entity, state, destination, start_time);

	// Re-enqueue attack-move only if still in motion
	if (move_time > time::time_t::from_int(0)) {
		command_queue->add_command(start_time,
		                           std::make_shared<component::command::AttackMoveCommand>(destination));
	}

	return move_time;
}

} // namespace openage::gamestate::system
