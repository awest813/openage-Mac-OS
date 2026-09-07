// Copyright 2026-2026 the openage authors. See copying.md for legal info.

#include "repair.h"

#include <cmath>

#include "log/log.h"
#include "log/message.h"

#include "gamestate/api/creatable.h"
#include "gamestate/component/api/live.h"
#include "gamestate/component/internal/command_queue.h"
#include "gamestate/component/internal/commands/repair.h"
#include "gamestate/component/internal/ownership.h"
#include "gamestate/component/internal/position.h"
#include "gamestate/component/types.h"
#include "gamestate/definitions.h"
#include "gamestate/game_entity.h"
#include "gamestate/game_state.h"
#include "gamestate/player.h"
#include "gamestate/system/move.h"


namespace openage::gamestate::system {

static constexpr const char *HP_ATTRIBUTE = "engine.ability.type.Live.AttributeAmount";

const time::time_t Repair::repair_command(
    const std::shared_ptr<gamestate::GameEntity> &entity,
    const std::shared_ptr<openage::gamestate::GameState> &state,
    const time::time_t &start_time) {
	auto command_queue = std::dynamic_pointer_cast<component::CommandQueue>(
		entity->get_component(component::component_t::COMMANDQUEUE));
	auto command = std::dynamic_pointer_cast<component::command::RepairCommand>(
		command_queue->pop_command(start_time));

	if (not command) [[unlikely]] {
		log::log(MSG(warn) << "Command is not a repair command.");
		return time::time_t::from_int(0);
	}

	const auto target_id = command->get_target();
	const auto &entities = state->get_game_entities();
	auto target_it = entities.find(target_id);
	if (target_it == entities.end()) [[unlikely]] {
		log::log(MSG(warn) << "Repair target " << target_id << " not found.");
		return time::time_t::from_int(0);
	}

	const auto &target = target_it->second;

	if (not target->has_component(component::component_t::LIVE)
	    || not target->has_component(component::component_t::POSITION)
	    || not entity->has_component(component::component_t::POSITION)) {
		return time::time_t::from_int(0);
	}

	// Ownership validation: only repair friendly / same-team entities
	player_id_t owner_id = 0;
	if (entity->has_component(component::component_t::OWNERSHIP)) {
		auto ownership = std::dynamic_pointer_cast<component::Ownership>(
			entity->get_component(component::component_t::OWNERSHIP));
		owner_id = ownership->get_owners().get(start_time);
	}

	if (target->has_component(component::component_t::OWNERSHIP)) {
		auto target_ownership = std::dynamic_pointer_cast<component::Ownership>(
			target->get_component(component::component_t::OWNERSHIP));
		player_id_t target_owner = target_ownership->get_owners().get(start_time);
		if (target_owner != owner_id) {
			log::log(MSG(dbg) << "Entity " << entity->get_id()
			                  << " cannot repair entity " << target_id
			                  << " owned by player " << target_owner << ".");
			return time::time_t::from_int(0);
		}
	}

	// Range check: walk to target if farther than REPAIR_INTERACTION_RANGE
	auto builder_pos_comp = std::dynamic_pointer_cast<component::Position>(
		entity->get_component(component::component_t::POSITION));
	auto target_pos_comp = std::dynamic_pointer_cast<component::Position>(
		target->get_component(component::component_t::POSITION));
	auto builder_pos = builder_pos_comp->get_positions().get(start_time);
	auto target_pos = target_pos_comp->get_positions().get(start_time);

	double dist = (target_pos - builder_pos).length();
	if (dist > REPAIR_INTERACTION_RANGE) {
		if (entity->has_component(component::component_t::MOVE)) {
			time::time_t move_time = Move::move_default(entity, state, target_pos, start_time);
			if (move_time > time::time_t::from_int(0)) {
				command_queue->add_command(
					start_time,
					std::make_shared<component::command::RepairCommand>(target_id));
				return move_time;
			}
		}
		return time::time_t::from_int(0);
	}

	// HP check
	auto live_component = std::dynamic_pointer_cast<component::Live>(
		target->get_component(component::component_t::LIVE));
	int64_t current_hp = live_component->get_attribute(start_time, HP_ATTRIBUTE);
	int64_t max_hp = state->get_entity_max_hp(target_id);

	if (current_hp >= max_hp) {
		log::log(MSG(info) << "Target " << target_id << " is already fully repaired ("
		                   << current_hp << "/" << max_hp << " HP).");
		return time::time_t::from_int(0);
	}

	// Determine repair rate: buildings repair at BUILDING_REPAIR_HP_PER_SEC (12.5 HP/s),
	// mobile units at UNIT_REPAIR_HP_PER_SEC (3.133 HP/s).
	bool is_building = not target->has_component(component::component_t::MOVE);
	double repair_rate = is_building ? BUILDING_REPAIR_HP_PER_SEC : UNIT_REPAIR_HP_PER_SEC;

	// Calculate repair step amount (nominal 1-second interval)
	double step_duration = 1.0;
	int64_t hp_to_repair = static_cast<int64_t>(std::ceil(repair_rate * step_duration));
	int64_t missing_hp = max_hp - current_hp;
	if (hp_to_repair > missing_hp) {
		hp_to_repair = missing_hp;
		step_duration = static_cast<double>(missing_hp) / repair_rate;
		if (step_duration < 0.1) {
			step_duration = 0.1;
		}
	}

	// Resource cost deduction: 0.5 * (build cost) / (build max hp) * (repaired hp)
	auto cost_opt = state->get_building_cost(target_id);
	if (cost_opt.has_value() and state->has_player(owner_id) and max_hp > 0) {
		auto &player = state->get_player(owner_id);
		const auto &cost_snapshot = cost_opt.value();

		double cost_fraction = 0.5 * static_cast<double>(hp_to_repair) / static_cast<double>(max_hp);
		BuildingCostRecord repair_cost;
		for (const auto &entry : cost_snapshot.entries) {
			int64_t resource_needed = static_cast<int64_t>(std::ceil(entry.amount * cost_fraction));
			if (resource_needed > 0) {
				repair_cost.entries.push_back(ResourceCostEntry{entry.resource, resource_needed});
			}
		}

		if (not api::player_can_afford(*player, repair_cost, start_time)) {
			log::log(MSG(dbg) << "Player " << owner_id
			                  << " cannot afford repair cost for " << target_id << ".");
			return time::time_t::from_int(0);
		}

		api::player_pay_cost(*player, repair_cost, start_time);
	}

	// Apply HP restoration
	int64_t new_hp = current_hp + hp_to_repair;
	if (new_hp > max_hp) {
		new_hp = max_hp;
	}
	live_component->set_attribute(start_time, HP_ATTRIBUTE, new_hp);

	log::log(MSG(info) << "Entity " << entity->get_id() << " repaired target " << target_id
	                   << " to " << new_hp << "/" << max_hp << " HP.");

	// If still damaged, re-enqueue repair command for next tick
	if (new_hp < max_hp) {
		command_queue->add_command(
			start_time,
			std::make_shared<component::command::RepairCommand>(target_id));
	}

	return time::time_t::from_double(step_duration);
}

} // namespace openage::gamestate::system
