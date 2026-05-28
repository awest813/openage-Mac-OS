// Copyright 2023-2026 the openage authors. See copying.md for legal info.

#include "idle.h"

#include <vector>

#include <nyan/nyan.h>

#include "error/error.h"
#include "log/log.h"
#include "log/message.h"

#include "gamestate/api/ability.h"
#include "gamestate/api/animation.h"
#include "gamestate/api/property.h"
#include "gamestate/api/types.h"
#include "gamestate/component/api/attack.h"
#include "gamestate/component/api/idle.h"
#include "gamestate/component/internal/command_queue.h"
#include "gamestate/component/internal/commands/attack.h"
#include "gamestate/component/internal/ownership.h"
#include "gamestate/component/internal/position.h"
#include "gamestate/component/internal/stance.h"
#include "gamestate/component/types.h"
#include "gamestate/game_entity.h"
#include "gamestate/game_state.h"


namespace openage::gamestate::system {

// Multiplier applied to attack range to compute the auto-attack vision radius
// for AGGRESSIVE stance.  Units scan this many times their attack range.
static constexpr double AGGRESSIVE_RANGE_MULTIPLIER = 5.0;

const time::time_t Idle::idle(const std::shared_ptr<gamestate::GameEntity> &entity,
                              const std::shared_ptr<openage::gamestate::GameState> &state,
                              const time::time_t &start_time) {
	if (not entity->has_component(component::component_t::IDLE)) [[unlikely]] {
		throw Error{ERR << "Entity " << entity->get_id() << " has no idle component."};
	}

	auto idle_component = std::dynamic_pointer_cast<component::Idle>(
		entity->get_component(component::component_t::IDLE));
	auto ability = idle_component->get_ability();
	if (api::APIAbility::check_property(ability, api::ability_property_t::ANIMATED)) {
		auto property = api::APIAbility::get_property(ability, api::ability_property_t::ANIMATED);
		auto animations = api::APIAbilityProperty::get_animations(property);
		auto animation_paths = api::APIAnimation::get_animation_paths(animations);

		if (animation_paths.size() > 0) [[likely]] {
			entity->render_update(start_time, animation_paths[0]);
		}
	}

	// Auto-attack: scan for enemies if the stance permits it.
	if (not entity->has_component(component::component_t::STANCE)
	    || not entity->has_component(component::component_t::ATTACK)
	    || not entity->has_component(component::component_t::POSITION)
	    || not entity->has_component(component::component_t::OWNERSHIP)) {
		return time::time_t::from_int(0);
	}

	auto stance_comp = std::dynamic_pointer_cast<component::Stance>(
		entity->get_component(component::component_t::STANCE));
	auto current_stance = stance_comp->get_stance(start_time);
	if (current_stance == component::stance_t::NO_ATTACK) {
		return time::time_t::from_int(0);
	}

	auto attack_comp = std::dynamic_pointer_cast<component::Attack>(
		entity->get_component(component::component_t::ATTACK));
	auto attack_ability = attack_comp->get_ability();
	auto max_range = attack_ability.get<nyan::Float>("Attack.max_range");
	double attack_range = max_range->get();

	double scan_radius = (current_stance == component::stance_t::AGGRESSIVE)
	                         ? attack_range * AGGRESSIVE_RANGE_MULTIPLIER
	                         : attack_range;

	auto pos_comp = std::dynamic_pointer_cast<component::Position>(
		entity->get_component(component::component_t::POSITION));
	auto own_pos = pos_comp->get_positions().get(start_time);

	auto own_owner_comp = std::dynamic_pointer_cast<component::Ownership>(
		entity->get_component(component::component_t::OWNERSHIP));
	auto own_owner = own_owner_comp->get_owners().get(start_time);

	for (const auto &[cand_id, candidate] : state->get_game_entities()) {
		if (cand_id == entity->get_id()) {
			continue;
		}
		if (not candidate->has_component(component::component_t::OWNERSHIP)
		    || not candidate->has_component(component::component_t::POSITION)
		    || not candidate->has_component(component::component_t::LIVE)) {
			continue;
		}

		auto cand_owner_comp = std::dynamic_pointer_cast<component::Ownership>(
			candidate->get_component(component::component_t::OWNERSHIP));
		if (cand_owner_comp->get_owners().get(start_time) == own_owner) {
			continue; // same team — skip
		}

		auto cand_pos_comp = std::dynamic_pointer_cast<component::Position>(
			candidate->get_component(component::component_t::POSITION));
		auto cand_pos = cand_pos_comp->get_positions().get(start_time);
		double dist = (cand_pos - own_pos).length();

		if (dist <= scan_radius) {
			// Found an enemy in range — push an ATTACK command.
			auto command_queue = std::dynamic_pointer_cast<component::CommandQueue>(
				entity->get_component(component::component_t::COMMANDQUEUE));
			command_queue->add_command(start_time,
			                           std::make_shared<component::command::AttackCommand>(cand_id));
			log::log(MSG(dbg) << "Entity " << entity->get_id()
			                  << " auto-targeting enemy " << cand_id
			                  << " (dist=" << dist << ").");
			break; // one target at a time
		}
	}

	// TODO: play sound

	return time::time_t::from_int(0);
}

} // namespace openage::gamestate::system
