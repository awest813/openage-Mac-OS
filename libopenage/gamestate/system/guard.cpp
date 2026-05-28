// Copyright 2026-2026 the openage authors. See copying.md for legal info.

#include "guard.h"

#include <nyan/nyan.h>

#include "log/log.h"
#include "log/message.h"

#include "gamestate/component/api/attack.h"
#include "gamestate/component/api/live.h"
#include "gamestate/component/internal/command_queue.h"
#include "gamestate/component/internal/commands/attack.h"
#include "gamestate/component/internal/commands/guard.h"
#include "gamestate/component/internal/ownership.h"
#include "gamestate/component/internal/position.h"
#include "gamestate/component/types.h"
#include "gamestate/game_entity.h"
#include "gamestate/game_state.h"
#include "gamestate/system/move.h"
#include "gamestate/types.h"


namespace openage::gamestate::system {

// Maximum distance to the guarded target before closing in (world units).
static constexpr double GUARD_RADIUS = 2.0;

// Polling interval when within guard radius and no enemies found.
static const time::time_t GUARD_SCAN_INTERVAL = time::time_t::from_double(0.5);


const time::time_t Guard::guard_command(
	const std::shared_ptr<gamestate::GameEntity> &entity,
	const std::shared_ptr<openage::gamestate::GameState> &state,
	const time::time_t &start_time) {
	// Pop the GUARD command from the queue
	auto command_queue = std::dynamic_pointer_cast<component::CommandQueue>(
		entity->get_component(component::component_t::COMMANDQUEUE));
	auto command = std::dynamic_pointer_cast<component::command::GuardCommand>(
		command_queue->pop_command(start_time));

	if (not command) [[unlikely]] {
		log::log(MSG(warn) << "Command is not a guard command.");
		return time::time_t::from_int(0);
	}

	entity_id_t target_id = command->get_target_id();

	// Check if the target entity still exists
	const auto &entities = state->get_game_entities();
	auto target_it = entities.find(target_id);
	if (target_it == entities.end()) {
		// Guard target has been destroyed — done
		return time::time_t::from_int(0);
	}

	const auto &target = target_it->second;

	// Get target position
	if (not target->has_component(component::component_t::POSITION)) {
		// Target has no position; nothing to guard
		return time::time_t::from_int(0);
	}

	auto target_pos_comp = std::dynamic_pointer_cast<component::Position>(
		target->get_component(component::component_t::POSITION));
	auto target_pos = target_pos_comp->get_positions().get(start_time);

	// Get own position
	if (not entity->has_component(component::component_t::POSITION)) {
		return time::time_t::from_int(0);
	}

	auto pos_comp = std::dynamic_pointer_cast<component::Position>(
		entity->get_component(component::component_t::POSITION));
	auto current_pos = pos_comp->get_positions().get(start_time);

	// Distance to target
	auto delta = target_pos - current_pos;
	double dist_to_target = delta.length();

	if (dist_to_target > GUARD_RADIUS) {
		// Move toward the guarded entity
		time::time_t move_time = Move::move_default(entity, state, target_pos, start_time);

		// Re-enqueue guard command
		command_queue->add_command(start_time,
		                           std::make_shared<component::command::GuardCommand>(target_id));

		return move_time;
	}

	// Within guard radius — scan for enemies in attack range

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
		// Attack the enemy, then resume guarding
		command_queue->add_command(start_time,
		                           std::make_shared<component::command::AttackCommand>(found_enemy));
		command_queue->add_command(start_time,
		                           std::make_shared<component::command::GuardCommand>(target_id));
		return time::time_t::from_int(0);
	}

	// No enemies found — re-enqueue guard command and wait
	command_queue->add_command(start_time,
	                           std::make_shared<component::command::GuardCommand>(target_id));

	return GUARD_SCAN_INTERVAL;
}

} // namespace openage::gamestate::system
