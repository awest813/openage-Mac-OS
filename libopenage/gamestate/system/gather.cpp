// Copyright 2024-2024 the openage authors. See copying.md for legal info.

#include "gather.h"

#include <optional>

#include <nyan/nyan.h>

#include "log/log.h"
#include "log/message.h"

#include "gamestate/api/ability.h"
#include "gamestate/api/animation.h"
#include "gamestate/api/property.h"
#include "gamestate/api/types.h"
#include "gamestate/component/api/gather.h"
#include "gamestate/component/api/live.h"
#include "gamestate/component/internal/command_queue.h"
#include "gamestate/component/internal/commands/gather.h"
#include "gamestate/component/internal/ownership.h"
#include "gamestate/component/internal/position.h"
#include "gamestate/component/internal/salvage.h"
#include "gamestate/component/types.h"
#include "gamestate/game_entity.h"
#include "gamestate/game_state.h"
#include "gamestate/player.h"


namespace openage::gamestate::system {

// nyan attribute fqon used to store current amount in a resource entity's Live component.
static constexpr const char *RESOURCE_AMOUNT_ATTRIBUTE = "engine.ability.type.Live.AttributeAmount";
// Distance in world-position units used for drop-off proximity checks.
static constexpr double DROPOFF_RANGE = 1.5;

namespace {

bool is_valid_dropoff_entity(const std::shared_ptr<gamestate::GameEntity> &entity,
                             player_id_t owner_id,
                             const std::shared_ptr<component::Position> &gatherer_pos_comp,
                             const time::time_t &time) {
	if (not entity->has_component(component::component_t::OWNERSHIP)
	    || not entity->has_component(component::component_t::POSITION)) {
		return false;
	}

	// For now, treat static owned entities as drop-off buildings.
	if (entity->has_component(component::component_t::MOVE)
	    || entity->has_component(component::component_t::GATHER)) {
		return false;
	}

	auto ownership = std::dynamic_pointer_cast<component::Ownership>(
		entity->get_component(component::component_t::OWNERSHIP));
	if (ownership->get_owners().get(time) != owner_id) {
		return false;
	}

	auto dropoff_pos_comp = std::dynamic_pointer_cast<component::Position>(
		entity->get_component(component::component_t::POSITION));
	auto gatherer_pos = gatherer_pos_comp->get_positions().get(time);
	auto dropoff_pos = dropoff_pos_comp->get_positions().get(time);
	auto delta = dropoff_pos - gatherer_pos;

	return delta.length() <= DROPOFF_RANGE;
}

bool try_drop_off(const std::shared_ptr<gamestate::GameEntity> &entity,
                  const std::shared_ptr<openage::gamestate::GameState> &state,
                  const time::time_t &time) {
	auto cargo = state->get_carried_resource(entity->get_id());
	if (not cargo.has_value()) {
		return false;
	}

	if (not entity->has_component(component::component_t::OWNERSHIP)
	    || not entity->has_component(component::component_t::POSITION)) {
		return false;
	}

	auto ownership = std::dynamic_pointer_cast<component::Ownership>(
		entity->get_component(component::component_t::OWNERSHIP));
	auto owner_id = ownership->get_owners().get(time);
	auto gatherer_pos_comp = std::dynamic_pointer_cast<component::Position>(
		entity->get_component(component::component_t::POSITION));

	if (not state->has_player(owner_id)) [[unlikely]] {
		log::log(MSG(warn) << "Gatherer " << entity->get_id()
		                   << " has unknown owner " << owner_id << "; cannot drop off.");
		return false;
	}

	for (const auto &[id, candidate] : state->get_game_entities()) {
		if (id == entity->get_id()) {
			continue;
		}
		if (not is_valid_dropoff_entity(candidate, owner_id, gatherer_pos_comp, time)) {
			continue;
		}

		auto &player = state->get_player(owner_id);
		player->add_resource(time, cargo->resource_type, cargo->amount);

		log::log(MSG(info) << "Entity " << entity->get_id()
		                   << " dropped off " << cargo->amount
		                   << " of " << cargo->resource_type
		                   << " at entity " << id << ".");
		state->clear_carried_resource(entity->get_id());
		return true;
	}

	return false;
}

} // namespace


const time::time_t Gather::gather_command(const std::shared_ptr<gamestate::GameEntity> &entity,
                                          const std::shared_ptr<openage::gamestate::GameState> &state,
                                          const time::time_t &start_time) {
	auto command_queue = std::dynamic_pointer_cast<component::CommandQueue>(
		entity->get_component(component::component_t::COMMANDQUEUE));
	auto command = std::dynamic_pointer_cast<component::command::GatherCommand>(
		command_queue->pop_command(start_time));

	if (not command) [[unlikely]] {
		log::log(MSG(warn) << "Command is not a gather command.");
		return time::time_t::from_int(0);
	}

	if (not entity->has_component(component::component_t::GATHER)) [[unlikely]] {
		log::log(WARN << "Entity " << entity->get_id() << " has no gather component.");
		return time::time_t::from_int(0);
	}

	// Gatherers carrying resources must first return to a drop-off building.
	if (state->is_carrying_resources(entity->get_id())) {
		if (try_drop_off(entity, state, start_time)) {
			return time::time_t::from_int(0);
		}

		log::log(MSG(dbg) << "Entity " << entity->get_id()
		                  << " is carrying resources and must return to a drop-off building.");
		return time::time_t::from_int(0);
	}

	// Retrieve nyan Gather ability data
	auto gather_component = std::dynamic_pointer_cast<component::Gather>(
		entity->get_component(component::component_t::GATHER));
	auto gather_ability = gather_component->get_ability();

	// Amount gathered per action
	auto gather_rate = gather_ability.get<nyan::Int>("Gather.gather_rate");
	// Maximum range to gather from
	auto max_range = gather_ability.get<nyan::Float>("Gather.max_range");
	// Reload time between gather actions (in seconds)
	auto reload_time = gather_ability.get<nyan::Float>("Gather.reload_time");
	// Resource type identifier (nyan fqon)
	auto resource_type = gather_ability.get<nyan::ObjectValue>("Gather.resource_type");

	auto target_id = command->get_target();

	// Look up the resource entity
	const auto &entities = state->get_game_entities();
	auto it = entities.find(target_id);
	if (it == entities.end()) [[unlikely]] {
		log::log(MSG(warn) << "Gather target " << target_id << " not found.");
		return time::time_t::from_int(0);
	}

	const auto &resource_entity = it->second;

	// Range check
	if (entity->has_component(component::component_t::POSITION)
	    && resource_entity->has_component(component::component_t::POSITION)) {
		auto gatherer_pos_comp = std::dynamic_pointer_cast<component::Position>(
			entity->get_component(component::component_t::POSITION));
		auto resource_pos_comp = std::dynamic_pointer_cast<component::Position>(
			resource_entity->get_component(component::component_t::POSITION));

		auto gatherer_pos = gatherer_pos_comp->get_positions().get(start_time);
		auto resource_pos = resource_pos_comp->get_positions().get(start_time);
		auto delta = resource_pos - gatherer_pos;
		double dist = delta.length();

		if (dist > max_range->get()) {
			log::log(MSG(dbg) << "Entity " << entity->get_id()
			                  << " is out of gather range of resource " << target_id
			                  << " (dist=" << dist << ", range=" << max_range->get() << ").");
			return time::time_t::from_int(0);
		}
	}

	int64_t current_amount = 0;
	int64_t new_amount = 0;
	nyan::fqon_t cargo_resource = resource_type->get_name();

	if (resource_entity->has_component(component::component_t::SALVAGE)) {
		auto salvage_component = std::dynamic_pointer_cast<component::Salvage>(
			resource_entity->get_component(component::component_t::SALVAGE));
		current_amount = salvage_component->get_amount(start_time);
		cargo_resource = nyan::fqon_t{salvage_component->get_resource_type()};
	}
	else if (resource_entity->has_component(component::component_t::LIVE)) {
		auto live_component = std::dynamic_pointer_cast<component::Live>(
			resource_entity->get_component(component::component_t::LIVE));
		current_amount = live_component->get_attribute(start_time, RESOURCE_AMOUNT_ATTRIBUTE);
	}
	else [[unlikely]] {
		log::log(MSG(warn) << "Gather target " << target_id
		                   << " has no live or salvage component.");
		return time::time_t::from_int(0);
	}

	int64_t gathered = gather_rate->get();
	if (gathered > current_amount) {
		gathered = current_amount;
	}
	new_amount = current_amount - gathered;

	if (resource_entity->has_component(component::component_t::SALVAGE)) {
		auto salvage_component = std::dynamic_pointer_cast<component::Salvage>(
			resource_entity->get_component(component::component_t::SALVAGE));
		salvage_component->set_amount(start_time, new_amount);
	}
	else {
		auto live_component = std::dynamic_pointer_cast<component::Live>(
			resource_entity->get_component(component::component_t::LIVE));
		live_component->set_attribute(start_time, RESOURCE_AMOUNT_ATTRIBUTE, new_amount);
	}

	// Store gathered resources as carried cargo.
	if (gathered > 0 && entity->has_component(component::component_t::OWNERSHIP)) {
		state->set_carried_resource(entity->get_id(), std::string{cargo_resource}, gathered);

		log::log(MSG(dbg) << "Entity " << entity->get_id()
		                  << " gathered and is now carrying " << gathered << " of " << cargo_resource
		                  << " from entity " << target_id
		                  << " (remaining=" << new_amount << ").");
	}

	// Depletion: remove resource entity when empty
	if (new_amount == 0) {
		log::log(MSG(info) << "Resource entity " << target_id << " has been depleted.");
		state->remove_game_entity(target_id);
		return time::time_t::from_int(0);
	}

	// Play gather animation if the ability has an ANIMATED property
	if (api::APIAbility::check_property(gather_ability, api::ability_property_t::ANIMATED)) {
		auto property = api::APIAbility::get_property(gather_ability, api::ability_property_t::ANIMATED);
		auto animations = api::APIAbilityProperty::get_animations(property);
		auto animation_paths = api::APIAnimation::get_animation_paths(animations);

		if (animation_paths.size() > 0) [[likely]] {
			entity->render_update(start_time, animation_paths[0]);
		}
	}

	return reload_time->get();
}

} // namespace openage::gamestate::system
