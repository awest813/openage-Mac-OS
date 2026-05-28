// Copyright 2023-2026 the openage authors. See copying.md for legal info.

#include "game_state.h"

#include <utility>

#include <nyan/nyan.h>

#include "error/error.h"
#include "event/event_loop.h"
#include "event/eventhandler.h"
#include "log/log.h"
#include "log/message.h"

#include "coord/tile.h"
#include "gamestate/component/internal/position.h"
#include "gamestate/component/types.h"
#include "gamestate/component/internal/ownership.h"
#include "gamestate/game_entity.h"
#include "gamestate/player.h"


namespace openage::gamestate {

GameState::GameState(const std::shared_ptr<nyan::Database> &db,
                     const std::shared_ptr<openage::event::EventLoop> &loop) :
	event::State{loop},
	event_loop{loop},
	db_view{db->new_view()} {
}

const std::shared_ptr<nyan::View> &GameState::get_db_view() {
	return this->db_view;
}

void GameState::add_game_entity(const std::shared_ptr<GameEntity> &entity) {
	if (this->game_entities.contains(entity->get_id())) [[unlikely]] {
		throw Error(MSG(err) << "Game entity with ID " << entity->get_id() << " already exists");
	}
	this->game_entities[entity->get_id()] = entity;
}

void GameState::remove_game_entity(entity_id_t id) {
	this->game_entities.erase(id);
	this->carried_resources.erase(id);
}

void GameState::remove_game_entity(entity_id_t id, const time::time_t &time) {
	// Identify the owner and whether the dying entity is a building,
	// before erasing it from the index.
	player_id_t owner_id = 0;
	bool is_building = false;

	auto it = this->game_entities.find(id);
	if (it != this->game_entities.end()) {
		auto &entity = it->second;

		if (entity->has_component(component::component_t::OWNERSHIP)) {
			auto ownership = std::dynamic_pointer_cast<component::Ownership>(
				entity->get_component(component::component_t::OWNERSHIP));
			owner_id = ownership->get_owners().get(time);

			// Heuristic: a building is an owned entity without a MOVE component.
			// TODO: classify buildings via an explicit nyan attribute/ability
			//       once a unit/building type system exists, instead of "no MOVE".
			is_building = not entity->has_component(component::component_t::MOVE);
		}
	}

	this->game_entities.erase(id);
	this->carried_resources.erase(id);

	if (is_building) {
		this->check_defeat(owner_id, time);
	}
}

void GameState::add_player(const std::shared_ptr<Player> &player) {
	if (this->players.contains(player->get_id())) [[unlikely]] {
		throw Error(MSG(err) << "Player with ID " << player->get_id() << " already exists");
	}
	this->players[player->get_id()] = player;
}

void GameState::set_map(const std::shared_ptr<Map> &map) {
	this->map = map;
}

const std::shared_ptr<GameEntity> &GameState::get_game_entity(entity_id_t id) const {
	if (!this->game_entities.contains(id)) [[unlikely]] {
		throw Error(MSG(err) << "Game entity with ID " << id << " does not exist");
	}
	return this->game_entities.at(id);
}

const std::unordered_map<entity_id_t, std::shared_ptr<GameEntity>> &GameState::get_game_entities() const {
	return this->game_entities;
}

const std::shared_ptr<Player> &GameState::get_player(player_id_t id) const {
	if (!this->players.contains(id)) [[unlikely]] {
		throw Error(MSG(err) << "Player with ID " << id << " does not exist");
	}
	return this->players.at(id);
}

bool GameState::has_player(player_id_t id) const {
	return this->players.contains(id);
}

const std::unordered_map<player_id_t, std::shared_ptr<Player>> &GameState::get_players() const {
	return this->players;
}

size_t GameState::get_alive_player_count() const {
	size_t count = 0;
	for (const auto &[id, player] : this->players) {
		if (player->get_state() == player_state_t::ALIVE) {
			++count;
		}
	}
	return count;
}

const std::shared_ptr<Map> &GameState::get_map() const {
	return this->map;
}

void GameState::check_defeat(player_id_t owner_id, const time::time_t &time) {
	// Skip players that are already out of the game.
	auto player_it = this->players.find(owner_id);
	if (player_it == this->players.end()) {
		return;
	}
	auto &player = player_it->second;
	if (player->get_state() != player_state_t::ALIVE) {
		return;
	}

	// Count surviving buildings for this player.
	size_t building_count = 0;
	for (const auto &[eid, entity] : this->game_entities) {
		if (not entity->has_component(component::component_t::OWNERSHIP)
		    || entity->has_component(component::component_t::MOVE)) {
			continue;
		}
		auto ownership = std::dynamic_pointer_cast<component::Ownership>(
			entity->get_component(component::component_t::OWNERSHIP));
		if (ownership->get_owners().get(time) == owner_id) {
			++building_count;
		}
	}

	if (building_count > 0) {
		return;
	}

	// No buildings left — player is defeated.
	player->set_state(player_state_t::DEFEATED);
	log::log(MSG(info) << "Player " << owner_id << " has been defeated.");

	if (this->event_loop) {
		this->event_loop->create_event(
			"game.player_defeated",
			nullptr,
			this->shared_from_this(),
			time,
			openage::event::EventHandler::param_map{{"player_id", owner_id}});
	}

	// Check if exactly one player remains alive — they win.
	player_id_t winner_id = 0;
	size_t alive_count = 0;
	for (const auto &[pid, p] : this->players) {
		if (p->get_state() == player_state_t::ALIVE) {
			++alive_count;
			winner_id = pid;
		}
	}

	if (alive_count == 1) {
		// Exactly one player remains — they win.
		this->players.at(winner_id)->set_state(player_state_t::WINNER);
		log::log(MSG(info) << "Player " << winner_id << " has won the game!");

		if (this->event_loop) {
			this->event_loop->create_event(
				"game.game_over",
				nullptr,
				this->shared_from_this(),
				time,
				openage::event::EventHandler::param_map{
					{"winner_id", winner_id},
					{"has_winner", true},
				});
		}
	}
	else if (alive_count == 0) {
		// No players remain (sole-player loss or simultaneous defeat):
		// the game is over with no winner.
		log::log(MSG(info) << "Game over — no players remain.");

		if (this->event_loop) {
			this->event_loop->create_event(
				"game.game_over",
				nullptr,
				this->shared_from_this(),
				time,
				openage::event::EventHandler::param_map{
					{"winner_id", player_id_t{0}},
					{"has_winner", false},
				});
		}
	}
}

void GameState::request_production(player_id_t owner,
                                   const std::string &game_entity,
                                   const time::time_t &completion_time) {
	this->production_requests.push_back(ProductionRequest{owner, game_entity, completion_time});
}

std::vector<ProductionRequest> GameState::take_completed_productions(const time::time_t &time) {
	std::vector<ProductionRequest> completed;
	std::vector<ProductionRequest> remaining;
	for (auto &request : this->production_requests) {
		if (request.completion_time <= time) {
			completed.push_back(request);
		}
		else {
			remaining.push_back(request);
		}
	}
	this->production_requests = std::move(remaining);

	return completed;
}

size_t GameState::pending_production_count() const {
	return this->production_requests.size();
}

bool GameState::is_carrying_resources(entity_id_t id) const {
	return this->carried_resources.contains(id);
}

std::optional<CarriedResource> GameState::get_carried_resource(entity_id_t id) const {
	auto it = this->carried_resources.find(id);
	if (it == this->carried_resources.end()) {
		return std::nullopt;
	}
	return it->second;
}

void GameState::set_carried_resource(entity_id_t id,
                                     const std::string &resource_type,
                                     int64_t amount) {
	this->carried_resources[id] = CarriedResource{resource_type, amount};
}

void GameState::clear_carried_resource(entity_id_t id) {
	this->carried_resources.erase(id);
}

const std::shared_ptr<assets::ModManager> &GameState::get_mod_manager() const {
	return this->mod_manager;
}

void GameState::set_mod_manager(const std::shared_ptr<assets::ModManager> &mod_manager) {
	this->mod_manager = mod_manager;
}

void GameState::update_player_visibility(player_id_t player,
                                         coord::tile center,
                                         int sight_range) {
	this->fog_of_war.update_visibility(player, center, sight_range);
}

void GameState::flush_player_visibility(player_id_t player) {
	this->fog_of_war.flush_visible(player);
}

bool GameState::is_tile_visible(player_id_t player, coord::tile tile) const {
	return this->fog_of_war.is_visible(player, tile);
}

bool GameState::is_tile_explored(player_id_t player, coord::tile tile) const {
	return this->fog_of_war.is_explored(player, tile);
}

bool GameState::is_entity_visible(player_id_t observer,
                                   entity_id_t entity_id,
                                   const time::time_t &time) const {
	auto it = this->game_entities.find(entity_id);
	if (it == this->game_entities.end()) {
		return false;
	}
	const auto &entity = it->second;
	if (not entity->has_component(component::component_t::POSITION)) {
		return false;
	}
	auto pos_comp = std::dynamic_pointer_cast<component::Position>(
		entity->get_component(component::component_t::POSITION));
	auto tile = pos_comp->get_positions().get(time).to_tile();
	return this->fog_of_war.is_visible(observer, tile);
}

} // namespace openage::gamestate
