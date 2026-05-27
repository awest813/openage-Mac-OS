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
#include "gamestate/component/internal/position.h"
#include "gamestate/component/types.h"
#include "gamestate/game_entity.h"
#include "gamestate/game_state.h"


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

	// Retrieve nyan Attack ability data
	auto attack_component = std::dynamic_pointer_cast<component::Attack>(
		attacker->get_component(component::component_t::ATTACK));
	auto attack_ability = attack_component->get_ability();

	// Damage dealt per attack
	auto damage = attack_ability.get<nyan::Int>("Attack.damage");
	// Maximum range of this attack
	auto max_range = attack_ability.get<nyan::Float>("Attack.max_range");
	// Reload time between attacks (in seconds)
	auto reload_time = attack_ability.get<nyan::Float>("Attack.reload_time");

	// Look up the target entity
	const auto &entities = state->get_game_entities();
	auto it = entities.find(target_id);
	if (it == entities.end()) [[unlikely]] {
		log::log(MSG(warn) << "Attack target " << target_id << " not found.");
		return time::time_t::from_int(0);
	}

	const auto &target = it->second;

	// Range check: only deal damage if the target is within attack range
	if (attacker->has_component(component::component_t::POSITION)
	    && target->has_component(component::component_t::POSITION)) {
		auto attacker_pos_comp = std::dynamic_pointer_cast<component::Position>(
			attacker->get_component(component::component_t::POSITION));
		auto target_pos_comp = std::dynamic_pointer_cast<component::Position>(
			target->get_component(component::component_t::POSITION));

		auto attacker_pos = attacker_pos_comp->get_positions().get(start_time);
		auto target_pos = target_pos_comp->get_positions().get(start_time);
		auto delta = target_pos - attacker_pos;
		double dist = delta.length();

		if (dist > max_range->get()) {
			log::log(MSG(dbg) << "Entity " << attacker->get_id()
			                  << " is out of attack range of target " << target_id
			                  << " (dist=" << dist << ", range=" << max_range->get() << ").");
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

	// Read current HP and subtract damage, clamping to 0
	int64_t current_hp = live_component->get_attribute(start_time, HP_ATTRIBUTE);
	int64_t new_hp = current_hp - damage->get();
	if (new_hp < 0) {
		new_hp = 0;
	}
	live_component->set_attribute(start_time, HP_ATTRIBUTE, new_hp);

	// Death detection: remove entity from the game state when HP reaches 0
	if (new_hp == 0) {
		log::log(MSG(info) << "Entity " << target_id << " has been destroyed.");
		state->remove_game_entity(target_id);
		// Reload time is irrelevant when target is dead; return 0 so the
		// attacker immediately re-evaluates its next command.
		return time::time_t::from_int(0);
	}

	// Play attack animation if the ability has an ANIMATED property
	if (api::APIAbility::check_property(attack_ability, api::ability_property_t::ANIMATED)) {
		auto property = api::APIAbility::get_property(attack_ability, api::ability_property_t::ANIMATED);
		auto animations = api::APIAbilityProperty::get_animations(property);
		auto animation_paths = api::APIAnimation::get_animation_paths(animations);

		if (animation_paths.size() > 0) [[likely]] {
			attacker->render_update(start_time, animation_paths[0]);
		}
	}

	return reload_time->get();
}

} // namespace openage::gamestate::system
