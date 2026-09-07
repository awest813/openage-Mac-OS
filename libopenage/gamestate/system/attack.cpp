// Copyright 2024-2024 the openage authors. See copying.md for legal info.

#include "attack.h"

#include <nyan/nyan.h>

#include "log/log.h"
#include "log/message.h"

#include "gamestate/api/ability.h"
#include "gamestate/api/animation.h"
#include "gamestate/api/property.h"
#include "gamestate/api/types.h"
#include "gamestate/component/api/attack.h"
#include "gamestate/component/api/live.h"
#include "gamestate/component/internal/command_queue.h"
#include "gamestate/component/internal/commands/attack.h"
#include "gamestate/component/internal/ownership.h"
#include "gamestate/component/internal/position.h"
#include "gamestate/component/types.h"
#include "gamestate/game_entity.h"
#include "gamestate/game_state.h"
#include "gamestate/player.h"


namespace openage::gamestate::system {

// nyan attribute fqon used to store current HP in the Live component.
static constexpr const char *HP_ATTRIBUTE = "engine.ability.type.Live.AttributeAmount";


const time::time_t Attack::attack_command(const std::shared_ptr<gamestate::GameEntity> &entity,
                                          const std::shared_ptr<openage::gamestate::GameState> &state,
                                          const time::time_t &start_time) {
	auto command_queue = std::dynamic_pointer_cast<component::CommandQueue>(
		entity->get_component(component::component_t::COMMANDQUEUE));
	auto command = std::dynamic_pointer_cast<component::command::AttackCommand>(
		command_queue->pop_command(start_time));

	if (not command) [[unlikely]] {
		log::log(MSG(warn) << "Command is not an attack command.");
		return time::time_t::from_int(0);
	}

	return Attack::attack_default(entity, command->get_target(), state, start_time);
}


const time::time_t Attack::attack_default(const std::shared_ptr<gamestate::GameEntity> &attacker,
                                          entity_id_t target_id,
                                          const std::shared_ptr<openage::gamestate::GameState> &state,
                                          const time::time_t &start_time) {
	if (not attacker->has_component(component::component_t::ATTACK)) [[unlikely]] {
		log::log(WARN << "Entity " << attacker->get_id() << " has no attack component.");
		return time::time_t::from_int(0);
	}

	auto attack_component = std::dynamic_pointer_cast<component::Attack>(
		attacker->get_component(component::component_t::ATTACK));

	int64_t dmg_val = attack_component->get_damage(start_time);
	double max_range_val = attack_component->get_max_range(start_time);
	double min_range_val = attack_component->get_min_range(start_time);
	double reload_time_val = attack_component->get_reload_time(start_time);
	double blast_radius_val = attack_component->get_blast_radius(start_time);
	armor_class_t attack_type = attack_component->get_attack_type(start_time);

	// Look up the target entity
	const auto &entities = state->get_game_entities();
	auto it = entities.find(target_id);
	if (it == entities.end()) [[unlikely]] {
		log::log(MSG(warn) << "Attack target " << target_id << " not found.");
		return time::time_t::from_int(0);
	}

	const auto &target = it->second;
	coord::phys3 attacker_pos{0, 0, 0};
	coord::phys3 target_pos{0, 0, 0};

	// Range checks
	if (attacker->has_component(component::component_t::POSITION)
	    && target->has_component(component::component_t::POSITION)) {
		auto attacker_pos_comp = std::dynamic_pointer_cast<component::Position>(
			attacker->get_component(component::component_t::POSITION));
		auto target_pos_comp = std::dynamic_pointer_cast<component::Position>(
			target->get_component(component::component_t::POSITION));

		attacker_pos = attacker_pos_comp->get_positions().get(start_time);
		target_pos = target_pos_comp->get_positions().get(start_time);
		auto delta = target_pos - attacker_pos;
		double dist = delta.length();

		// Minimum range check (skirmishers, trebuchets, mangonels cannot attack inside min range)
		if (dist < min_range_val) {
			log::log(MSG(dbg) << "Entity " << attacker->get_id()
			                  << " is within minimum attack range of target " << target_id
			                  << " (dist=" << dist << ", min_range=" << min_range_val << ").");
			return time::time_t::from_double(reload_time_val);
		}

		// Maximum range check: approach target if too far
		if (dist > max_range_val) {
			if (attacker->has_component(component::component_t::MOVE)
			    and attacker->has_component(component::component_t::COMMANDQUEUE)) {
				time::time_t move_time = Move::move_default(attacker, state, target_pos, start_time);
				if (move_time > time::time_t::from_int(0)) {
					auto command_queue = std::dynamic_pointer_cast<component::CommandQueue>(
						attacker->get_component(component::component_t::COMMANDQUEUE));
					command_queue->add_command(
						start_time,
						std::make_shared<component::command::AttackCommand>(target_id));
					return move_time;
				}
			}

			log::log(MSG(dbg) << "Entity " << attacker->get_id()
			                  << " is out of attack range of target " << target_id
			                  << " (dist=" << dist << ", range=" << max_range_val << ").");
			return time::time_t::from_int(0);
		}
	}

	// Apply damage to target's HP via the Live component
	if (not target->has_component(component::component_t::LIVE)) [[unlikely]] {
		log::log(MSG(warn) << "Attack target " << target_id << " has no live component.");
		return time::time_t::from_int(0);
	}

	auto live_component = std::dynamic_pointer_cast<component::Live>(
		target->get_component(component::component_t::LIVE));

	// 1. Base damage after target's primary armor
	int64_t target_melee_armor = state->get_entity_armor(target_id, armor_class_t::MELEE);
	int64_t target_pierce_armor = state->get_entity_armor(target_id, armor_class_t::PIERCE);
	int64_t base_damage = 0;
	if (attack_type == armor_class_t::MELEE) {
		base_damage = std::max<int64_t>(0, dmg_val - target_melee_armor);
	}
	else {
		base_damage = std::max<int64_t>(0, dmg_val - target_pierce_armor);
	}

	// 2. Attack bonuses against target's armor classes
	int64_t bonus_sum = 0;
	for (const auto &bonus : state->get_entity_attack_bonuses(attacker->get_id())) {
		int64_t target_resist = state->get_entity_armor(target_id, bonus.armor_class);
		if (bonus.bonus_damage > target_resist) {
			bonus_sum += (bonus.bonus_damage - target_resist);
		}
	}

	// 3. Elevation factor (+25% downhill / high ground, -25% uphill / low ground)
	double elevation_factor = 1.0;
	if (attacker_pos.up > target_pos.up + coord::phys_t{ELEVATION_THRESHOLD}) {
		elevation_factor = ELEVATION_DAMAGE_BONUS;
	}
	else if (attacker_pos.up < target_pos.up - coord::phys_t{ELEVATION_THRESHOLD}) {
		elevation_factor = ELEVATION_DAMAGE_MALUS;
	}

	// 4. Effective damage per projectile (minimum 1 damage floor)
	int64_t effective_damage = static_cast<int64_t>(
		std::round((base_damage + bonus_sum) * elevation_factor));
	effective_damage = std::max<int64_t>(1, effective_damage);

	// 5. Additional arrows from garrisoned units
	int64_t total_damage = effective_damage;
	int64_t extra_arrows = state->get_building_additional_arrows(attacker->get_id(), start_time);
	if (extra_arrows > 0) {
		total_damage += extra_arrows * effective_damage;
	}

	// Read current HP and subtract damage, clamping to 0
	int64_t current_hp = live_component->get_attribute(start_time, HP_ATTRIBUTE);
	int64_t new_hp = current_hp - total_damage;
	if (new_hp < 0) {
		new_hp = 0;
	}
	live_component->set_attribute(start_time, HP_ATTRIBUTE, new_hp);

	// Area splash / blast damage for siege units
	if (blast_radius_val > 0.0) {
		for (const auto &[sec_id, sec_entity] : state->get_game_entities()) {
			if (sec_id == target_id or sec_id == attacker->get_id()) {
				continue;
			}
			if (not sec_entity->has_component(component::component_t::POSITION)
			    or not sec_entity->has_component(component::component_t::LIVE)) {
				continue;
			}

			auto sec_pos_comp = std::dynamic_pointer_cast<component::Position>(
				sec_entity->get_component(component::component_t::POSITION));
			auto sec_pos = sec_pos_comp->get_positions().get(start_time);
			double sec_dist = (sec_pos - target_pos).length();

			if (sec_dist <= blast_radius_val) {
				double falloff = 1.0 - (sec_dist / (2.0 * blast_radius_val));
				int64_t splash_dmg = std::max<int64_t>(1, static_cast<int64_t>(std::floor(effective_damage * falloff)));

				auto sec_live = std::dynamic_pointer_cast<component::Live>(
					sec_entity->get_component(component::component_t::LIVE));
				int64_t sec_hp = sec_live->get_attribute(start_time, HP_ATTRIBUTE);
				int64_t next_sec_hp = std::max<int64_t>(0, sec_hp - splash_dmg);
				sec_live->set_attribute(start_time, HP_ATTRIBUTE, next_sec_hp);

				if (next_sec_hp == 0) {
					if (attacker->has_component(component::component_t::OWNERSHIP)) {
						auto ownership = std::dynamic_pointer_cast<component::Ownership>(
							attacker->get_component(component::component_t::OWNERSHIP));
						auto attacker_owner = ownership->get_owners().get(start_time);
						if (state->has_player(attacker_owner)) {
							state->get_player(attacker_owner)->record_kill(start_time);
						}
					}
					state->remove_game_entity(sec_id, start_time);
				}
			}
		}
	}

	// Death detection: remove entity from the game state when HP reaches 0
	if (new_hp == 0) {
		log::log(MSG(info) << "Entity " << target_id << " has been destroyed.");

		// Credit the attacker's owner with a kill (after-game statistics).
		if (attacker->has_component(component::component_t::OWNERSHIP)) {
			auto ownership = std::dynamic_pointer_cast<component::Ownership>(
				attacker->get_component(component::component_t::OWNERSHIP));
			auto attacker_owner = ownership->get_owners().get(start_time);
			if (state->has_player(attacker_owner)) {
				state->get_player(attacker_owner)->record_kill(start_time);
			}
		}

		state->remove_game_entity(target_id, start_time);
		return time::time_t::from_int(0);
	}

	// Play attack animation if nyan ability has an ANIMATED property
	try {
		auto attack_ability = attack_component->get_ability();
		if (api::APIAbility::check_property(attack_ability, api::ability_property_t::ANIMATED)) {
			auto property = api::APIAbility::get_property(attack_ability, api::ability_property_t::ANIMATED);
			auto animations = api::APIAbilityProperty::get_animations(property);
			auto animation_paths = api::APIAnimation::get_animation_paths(animations);

			if (animation_paths.size() > 0) [[likely]] {
				attacker->render_update(start_time, animation_paths[0]);
			}
		}
	}
	catch (...) {
	}

	// If target survives and no other command was queued, continue attacking it.
	if (new_hp > 0 and attacker->has_component(component::component_t::COMMANDQUEUE)) {
		auto command_queue = std::dynamic_pointer_cast<component::CommandQueue>(
			attacker->get_component(component::component_t::COMMANDQUEUE));
		if (command_queue->get_queue().empty(start_time)) {
			command_queue->add_command(
				start_time,
				std::make_shared<component::command::AttackCommand>(target_id));
		}
	}

	return time::time_t::from_double(reload_time_val);
}

} // namespace openage::gamestate::system
