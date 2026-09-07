// Copyright 2026-2026 the openage authors. See copying.md for legal info.

#include "trade.h"

#include <cmath>
#include <limits>

#include "log/log.h"
#include "log/message.h"

#include "gamestate/component/internal/command_queue.h"
#include "gamestate/component/internal/commands/trade.h"
#include "gamestate/component/internal/ownership.h"
#include "gamestate/component/internal/position.h"
#include "gamestate/component/types.h"
#include "gamestate/definitions.h"
#include "gamestate/game_entity.h"
#include "gamestate/game_state.h"
#include "gamestate/player.h"
#include "gamestate/system/move.h"


namespace openage::gamestate::system {

const time::time_t Trade::trade_command(
    const std::shared_ptr<gamestate::GameEntity> &entity,
    const std::shared_ptr<openage::gamestate::GameState> &state,
    const time::time_t &start_time) {
	auto command_queue = std::dynamic_pointer_cast<component::CommandQueue>(
		entity->get_component(component::component_t::COMMANDQUEUE));
	auto command = std::dynamic_pointer_cast<component::command::TradeCommand>(
		command_queue->pop_command(start_time));

	if (not command) [[unlikely]] {
		log::log(MSG(warn) << "Command is not a trade command.");
		return time::time_t::from_int(0);
	}

	if (not entity->has_component(component::component_t::POSITION)) {
		return time::time_t::from_int(0);
	}

	player_id_t owner_id = 0;
	if (entity->has_component(component::component_t::OWNERSHIP)) {
		auto ownership = std::dynamic_pointer_cast<component::Ownership>(
			entity->get_component(component::component_t::OWNERSHIP));
		owner_id = ownership->get_owners().get(start_time);
	}

	const auto &entities = state->get_game_entities();
	entity_id_t target_market = command->get_target_market();
	entity_id_t home_market = command->get_home_market();
	auto trade_state = command->get_state();

	// Resolve or validate home market
	if (home_market == 0 or not entities.contains(home_market)) {
		auto friendly_markets = state->get_friendly_markets(owner_id);
		if (friendly_markets.empty()) {
			// No friendly markets available: drop any cargo and stop
			state->clear_carried_resource(entity->get_id());
			return time::time_t::from_int(0);
		}

		auto unit_pos_comp = std::dynamic_pointer_cast<component::Position>(
			entity->get_component(component::component_t::POSITION));
		auto unit_pos = unit_pos_comp->get_positions().get(start_time);

		entity_id_t closest_home = 0;
		double min_dist = std::numeric_limits<double>::max();
		for (entity_id_t fm_id : friendly_markets) {
			auto fit = entities.find(fm_id);
			if (fit != entities.end() && fit->second->has_component(component::component_t::POSITION)) {
				auto fpos_comp = std::dynamic_pointer_cast<component::Position>(
					fit->second->get_component(component::component_t::POSITION));
				double d = (fpos_comp->get_positions().get(start_time) - unit_pos).length();
				if (d < min_dist) {
					min_dist = d;
					closest_home = fm_id;
				}
			}
		}
		home_market = closest_home;
		command->set_home_market(home_market);
	}

	bool target_alive = entities.contains(target_market);
	if (not target_alive) {
		// Target market destroyed: if cart carries cargo, return home to deposit; otherwise stop
		if (state->is_carrying_resources(entity->get_id())) {
			trade_state = component::command::trade_state_t::RETURNING_HOME;
		}
		else {
			return time::time_t::from_int(0);
		}
	}

	auto unit_pos_comp = std::dynamic_pointer_cast<component::Position>(
		entity->get_component(component::component_t::POSITION));
	auto unit_pos = unit_pos_comp->get_positions().get(start_time);

	if (trade_state == component::command::trade_state_t::TO_TARGET) {
		auto target_it = entities.find(target_market);
		if (target_it == entities.end() or not target_it->second->has_component(component::component_t::POSITION)) {
			return time::time_t::from_int(0);
		}

		auto target_pos_comp = std::dynamic_pointer_cast<component::Position>(
			target_it->second->get_component(component::component_t::POSITION));
		auto target_pos = target_pos_comp->get_positions().get(start_time);

		double dist = (target_pos - unit_pos).length();
		if (dist > TRADE_INTERACTION_RANGE) {
			if (entity->has_component(component::component_t::MOVE)) {
				time::time_t move_time = Move::move_default(entity, state, target_pos, start_time);
				if (move_time > time::time_t::from_int(0)) {
					command_queue->add_command(
						start_time,
						std::make_shared<component::command::TradeCommand>(
							target_market, home_market, component::command::trade_state_t::TO_TARGET));
					return move_time;
				}
			}
			return time::time_t::from_int(0);
		}

		// Reached target market: calculate gold payload based on distance between markets
		auto home_it = entities.find(home_market);
		if (home_it != entities.end() and home_it->second->has_component(component::component_t::POSITION)) {
			auto home_pos_comp = std::dynamic_pointer_cast<component::Position>(
				home_it->second->get_component(component::component_t::POSITION));
			auto home_pos = home_pos_comp->get_positions().get(start_time);

			int64_t gold_payload = state->calculate_trade_gold(home_pos, target_pos);
			std::string gold_fqon = state->get_market_resource_fqon(market_resource_t::GOLD);
			state->set_carried_resource(entity->get_id(), gold_fqon, gold_payload);

			// Step towards home and re-enqueue in returning state
			if (entity->has_component(component::component_t::MOVE)) {
				time::time_t move_time = Move::move_default(entity, state, home_pos, start_time);
				command_queue->add_command(
					start_time,
					std::make_shared<component::command::TradeCommand>(
						target_market, home_market, component::command::trade_state_t::RETURNING_HOME));
				return move_time;
			}
		}

		return time::time_t::from_int(0);
	}
	else { // RETURNING_HOME
		auto home_it = entities.find(home_market);
		if (home_it == entities.end() or not home_it->second->has_component(component::component_t::POSITION)) {
			state->clear_carried_resource(entity->get_id());
			return time::time_t::from_int(0);
		}

		auto home_pos_comp = std::dynamic_pointer_cast<component::Position>(
			home_it->second->get_component(component::component_t::POSITION));
		auto home_pos = home_pos_comp->get_positions().get(start_time);

		double dist = (home_pos - unit_pos).length();
		if (dist > TRADE_INTERACTION_RANGE) {
			if (entity->has_component(component::component_t::MOVE)) {
				time::time_t move_time = Move::move_default(entity, state, home_pos, start_time);
				if (move_time > time::time_t::from_int(0)) {
					command_queue->add_command(
						start_time,
						std::make_shared<component::command::TradeCommand>(
							target_market, home_market, component::command::trade_state_t::RETURNING_HOME));
					return move_time;
				}
			}
			return time::time_t::from_int(0);
		}

		// Reached home market: unload gold payload
		auto cargo = state->get_carried_resource(entity->get_id());
		if (cargo.has_value() and state->has_player(owner_id)) {
			auto &player = state->get_player(owner_id);
			player->add_resource(start_time, cargo->resource_type, cargo->amount);
			player->record_trade_profit(start_time, cargo->amount);
			state->clear_carried_resource(entity->get_id());
		}

		// If target market is still alive, head back to target for another trade run!
		if (target_alive) {
			auto target_it = entities.find(target_market);
			if (target_it != entities.end() and target_it->second->has_component(component::component_t::POSITION)) {
				auto target_pos_comp = std::dynamic_pointer_cast<component::Position>(
					target_it->second->get_component(component::component_t::POSITION));
				auto target_pos = target_pos_comp->get_positions().get(start_time);

				if (entity->has_component(component::component_t::MOVE)) {
					time::time_t move_time = Move::move_default(entity, state, target_pos, start_time);
					command_queue->add_command(
						start_time,
						std::make_shared<component::command::TradeCommand>(
							target_market, home_market, component::command::trade_state_t::TO_TARGET));
					return move_time;
				}
			}
		}

		// Target was destroyed while returning; stop at home market
		return time::time_t::from_int(0);
	}
}

} // namespace openage::gamestate::system
