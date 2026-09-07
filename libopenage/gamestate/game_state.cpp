// Copyright 2023-2026 the openage authors. See copying.md for legal info.

#include "game_state.h"

#include <algorithm>
#include <shared_mutex>
#include <cmath>
#include <utility>
#include <vector>

#include <nyan/nyan.h>

#include "error/error.h"
#include "event/event_loop.h"
#include "event/eventhandler.h"
#include "log/log.h"
#include "log/message.h"

#include "coord/tile.h"
#include "gamestate/api/creatable.h"
#include "gamestate/component/api/attack.h"
#include "gamestate/component/api/live.h"
#include "gamestate/component/internal/position.h"
#include "gamestate/component/internal/command_queue.h"
#include "gamestate/component/internal/commands/base_command.h"
#include "gamestate/component/internal/commands/garrison.h"
#include "gamestate/component/internal/ownership.h"
#include "gamestate/component/internal/position.h"
#include "gamestate/component/internal/salvage.h"
#include "pathfinding/cost_field.h"
#include "pathfinding/definitions.h"
#include "pathfinding/grid.h"
#include "pathfinding/sector.h"
#include "gamestate/game_entity.h"
#include "gamestate/map.h"
#include "gamestate/definitions.h"
#include "gamestate/player.h"
#include "gamestate/terrain.h"
#include "gamestate/terrain_chunk.h"
#include "gamestate/terrain_tile.h"
#include "renderer/stages/world/render_entity.h"

#include <cctype>
#include <string>


namespace openage::gamestate {

GameState::GameState(const std::shared_ptr<nyan::Database> &db,
                     const std::shared_ptr<openage::event::EventLoop> &loop) :
	event::State{loop},
	event_loop{loop},
	db_view{db->new_view()},
	view_player_id{0} {
	this->market_base_prices[market_resource_t::WOOD] = MARKET_DEFAULT_BASE_PRICE_WOOD;
	this->market_base_prices[market_resource_t::FOOD] = MARKET_DEFAULT_BASE_PRICE_FOOD;
	this->market_base_prices[market_resource_t::STONE] = MARKET_DEFAULT_BASE_PRICE_STONE;
	this->market_resource_fqons[market_resource_t::FOOD] = "test.resource.Food";
	this->market_resource_fqons[market_resource_t::WOOD] = "test.resource.Wood";
	this->market_resource_fqons[market_resource_t::STONE] = "test.resource.Stone";
	this->market_resource_fqons[market_resource_t::GOLD] = "test.resource.Gold";

	// Initialize standard AoE2 technology definitions
	this->register_tech_definition({
		.tech_id = TECH_FEUDAL_AGE,
		.name = "Feudal Age",
		.required_age = age_t::DARK_AGE,
		.research_time_sec = FEUDAL_AGE_RESEARCH_TIME_SEC,
		.cost = {{"test.resource.Food", FEUDAL_AGE_COST_FOOD}},
		.effect = [](GameState *gs, player_id_t pid, const time::time_t &t) {
			if (auto player = gs->get_player(pid)) {
				player->set_age(t, age_t::FEUDAL_AGE);
			}
		}
	});

	this->register_tech_definition({
		.tech_id = TECH_CASTLE_AGE,
		.name = "Castle Age",
		.required_age = age_t::FEUDAL_AGE,
		.research_time_sec = CASTLE_AGE_RESEARCH_TIME_SEC,
		.cost = {{"test.resource.Food", CASTLE_AGE_COST_FOOD}, {"test.resource.Gold", CASTLE_AGE_COST_GOLD}},
		.effect = [](GameState *gs, player_id_t pid, const time::time_t &t) {
			if (auto player = gs->get_player(pid)) {
				player->set_age(t, age_t::CASTLE_AGE);
			}
		}
	});

	this->register_tech_definition({
		.tech_id = TECH_IMPERIAL_AGE,
		.name = "Imperial Age",
		.required_age = age_t::CASTLE_AGE,
		.research_time_sec = IMPERIAL_AGE_RESEARCH_TIME_SEC,
		.cost = {{"test.resource.Food", IMPERIAL_AGE_COST_FOOD}, {"test.resource.Gold", IMPERIAL_AGE_COST_GOLD}},
		.effect = [](GameState *gs, player_id_t pid, const time::time_t &t) {
			if (auto player = gs->get_player(pid)) {
				player->set_age(t, age_t::IMPERIAL_AGE);
			}
		}
	});

	this->register_tech_definition({
		.tech_id = TECH_LOOM,
		.name = "Loom",
		.required_age = age_t::DARK_AGE,
		.research_time_sec = LOOM_RESEARCH_TIME_SEC,
		.cost = {{"test.resource.Gold", LOOM_COST_GOLD}},
		.effect = nullptr
	});

	this->register_tech_definition({
		.tech_id = TECH_COINAGE,
		.name = "Coinage",
		.required_age = age_t::CASTLE_AGE,
		.research_time_sec = 70.0,
		.cost = {{"test.resource.Food", 200}, {"test.resource.Gold", 100}},
		.effect = [](GameState *gs, player_id_t pid, const time::time_t &) {
			gs->set_tribute_fee(pid, TRIBUTE_FEE_COINAGE);
		}
	});

	this->register_tech_definition({
		.tech_id = TECH_BANKING,
		.name = "Banking",
		.required_age = age_t::IMPERIAL_AGE,
		.research_time_sec = 70.0,
		.cost = {{"test.resource.Food", 300}, {"test.resource.Gold", 200}},
		.effect = [](GameState *gs, player_id_t pid, const time::time_t &) {
			gs->set_tribute_fee(pid, TRIBUTE_FEE_BANKING);
		}
	});

	this->register_tech_definition({
		.tech_id = TECH_GUILDS,
		.name = "Guilds",
		.required_age = age_t::IMPERIAL_AGE,
		.research_time_sec = 50.0,
		.cost = {{"test.resource.Food", 300}, {"test.resource.Gold", 200}},
		.effect = [](GameState *gs, player_id_t pid, const time::time_t &) {
			gs->set_market_fee(pid, MARKET_FEE_GUILDS);
		}
	});

	this->register_tech_definition({
		.tech_id = TECH_FORGING,
		.name = "Forging",
		.required_age = age_t::FEUDAL_AGE,
		.research_time_sec = 50.0,
		.cost = {{"test.resource.Food", 150}},
		.effect = nullptr
	});
}

const std::shared_ptr<nyan::View> &GameState::get_db_view() {
	return this->db_view;
}

void GameState::add_game_entity(const std::shared_ptr<GameEntity> &entity) {
	if (this->game_entities.contains(entity->get_id())) [[unlikely]] {
		throw Error(MSG(err) << "Game entity with ID " << entity->get_id() << " already exists");
	}
	this->game_entities[entity->get_id()] = entity;
	if (entity->has_component(component::component_t::SALVAGE)) {
		this->salvage_pile_ids.insert(entity->get_id());
	}
}

void GameState::remove_game_entity(entity_id_t id) {
	this->unregister_street_by_entity(id);
	this->unregister_bridge_by_entity(id);
	this->fog_of_war.clear_entity(id);
	this->game_entities.erase(id);
	this->carried_resources.erase(id);
	this->rally_points.erase(id);
	this->building_costs.erase(id);
	this->entity_population_demand.erase(id);
	this->entity_population_provision.erase(id);
	this->salvage_pile_ids.erase(id);
	this->resource_nodes.erase(id);
	this->market_entities.erase(id);
	auto res_it1 = this->building_research.find(id);
	if (res_it1 != this->building_research.end()) {
		player_id_t p_id = res_it1->second.player_id;
		int64_t t_id = res_it1->second.tech_id;
		this->active_techs_in_progress[p_id].erase(t_id);
		this->building_research.erase(res_it1);
	}
	this->release_tile(id);
}

void GameState::remove_game_entity(entity_id_t id, const time::time_t &time) {
	// Identify the owner and whether the dying entity is a building,
	// before erasing it from the index.
	player_id_t owner_id = 0;
	bool is_building = false;
	bool is_owned_unit = false;

	coord::phys3 building_pos = WORLD_ORIGIN;
	bool have_building_pos = false;
	std::optional<BuildingCostRecord> building_cost;
	std::optional<int64_t> population_demand;
	std::optional<int64_t> population_provision;

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
			is_owned_unit = not is_building;
		}

		if (is_building && entity->has_component(component::component_t::POSITION)) {
			auto pos_comp = std::dynamic_pointer_cast<component::Position>(
				entity->get_component(component::component_t::POSITION));
			building_pos = pos_comp->get_positions().get(time);
			have_building_pos = true;
		}

		building_cost = this->get_building_cost(id);
	}

	population_demand = this->get_entity_population_demand(id);
	population_provision = this->get_entity_population_provision(id);

	this->unregister_street_by_entity(id);
	this->unregister_bridge_by_entity(id);
	this->fog_of_war.clear_entity(id);

	this->game_entities.erase(id);
	this->carried_resources.erase(id);
	this->rally_points.erase(id);
	this->building_costs.erase(id);
	this->entity_population_demand.erase(id);
	this->entity_population_provision.erase(id);
	this->entity_max_hp.erase(id);
	this->salvage_pile_ids.erase(id);
	this->resource_nodes.erase(id);
	this->market_entities.erase(id);
	auto res_it2 = this->building_research.find(id);
	if (res_it2 != this->building_research.end()) {
		player_id_t p_id = res_it2->second.player_id;
		int64_t t_id = res_it2->second.tech_id;
		this->active_techs_in_progress[p_id].erase(t_id);
		this->building_research.erase(res_it2);
	}
	this->release_tile(id);

	// If the removed entity was a garrison container (e.g. building/ram), eject units
	if (this->building_garrisons.contains(id)) {
		this->ungarrison_entities(id, time);
	}

	// If the removed entity was itself garrisoned, unregister from parent
	auto parent_it = this->unit_garrison_parent.find(id);
	if (parent_it != this->unit_garrison_parent.end()) {
		entity_id_t parent_id = parent_it->second;
		auto garr_it = this->building_garrisons.find(parent_id);
		if (garr_it != this->building_garrisons.end()) {
			auto &vec = garr_it->second;
			vec.erase(std::remove(vec.begin(), vec.end(), id), vec.end());
			if (vec.empty()) {
				this->building_garrisons.erase(garr_it);
			}
		}
		this->unit_garrison_parent.erase(parent_it);
	}

	this->garrison_capacities.erase(id);
	this->saved_tasks.erase(id);
	this->entity_armors.erase(id);
	this->entity_attack_bonuses.erase(id);

	// Record the loss of an owned entity for the after-game statistics.
	if ((is_owned_unit or is_building) and this->has_player(owner_id)) {
		this->get_player(owner_id)->record_loss(time);
	}

	// Release the population space a unit reserved when it was trained.
	// Only release when demand was recorded — units without a recorded cost
	// (e.g. 0-pop or test helpers that never called set_entity_population_demand)
	// must not invent a phantom DEFAULT_POPULATION_COST debit.
	if (is_owned_unit and this->has_player(owner_id) and population_demand.has_value()) {
		int64_t demand = population_demand.value();
		if (demand > 0) {
			this->get_player(owner_id)->add_population_demand(time, -demand);
		}
	}

	// Remove the population headroom a destroyed building had provided.
	// Only release when provision was recorded at spawn — buildings without
	// ProvideContingent never raised capacity and must not subtract the default.
	if (is_building and this->has_player(owner_id) and population_provision.has_value()) {
		int64_t provision = population_provision.value();
		if (provision > 0) {
			this->get_player(owner_id)->add_population_capacity(time, -provision);
		}
	}

	if (is_building) {
		if (have_building_pos && building_cost.has_value()) {
			this->spawn_salvage_pile(building_pos,
			                         building_cost.value(),
			                         building_cost->destroy_recovery_fraction,
			                         time);
		}
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

void GameState::set_game_result(GameResult result) {
	std::unique_lock lock{this->game_result_mutex};
	this->game_result = std::move(result);
}

GameResult GameState::get_game_result() const {
	std::shared_lock lock{this->game_result_mutex};
	return this->game_result;
}

void GameState::clear_game_result() {
	std::unique_lock lock{this->game_result_mutex};
	this->game_result = GameResult{};
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
		this->set_game_result(GameResult{true, true, winner_id});

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
		this->set_game_result(GameResult{true, false, player_id_t{0}});

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

bool GameState::has_rally_point(entity_id_t id) const {
	return this->rally_points.contains(id);
}

std::optional<coord::phys3> GameState::get_rally_point(entity_id_t id) const {
	auto it = this->rally_points.find(id);
	if (it == this->rally_points.end()) {
		return std::nullopt;
	}
	return it->second;
}

void GameState::set_rally_point(entity_id_t id, const coord::phys3 &target) {
	// coord::phys3 is not default-constructible, so operator[] cannot be used.
	this->rally_points.insert_or_assign(id, target);
}

void GameState::clear_rally_point(entity_id_t id) {
	this->rally_points.erase(id);
}

void GameState::set_building_cost(entity_id_t id, BuildingCostRecord cost) {
	this->building_costs[id] = api::normalize_building_cost(std::move(cost));
}

std::optional<BuildingCostRecord> GameState::get_building_cost(entity_id_t id) const {
	auto it = this->building_costs.find(id);
	if (it == this->building_costs.end()) {
		return std::nullopt;
	}
	return it->second;
}

void GameState::clear_building_cost(entity_id_t id) {
	this->building_costs.erase(id);
}

void GameState::set_entity_population_demand(entity_id_t id, int64_t amount) {
	if (amount > 0) {
		this->entity_population_demand[id] = amount;
	}
}

std::optional<int64_t> GameState::get_entity_population_demand(entity_id_t id) const {
	auto it = this->entity_population_demand.find(id);
	if (it == this->entity_population_demand.end()) {
		return std::nullopt;
	}
	return it->second;
}

void GameState::set_entity_population_provision(entity_id_t id, int64_t amount) {
	if (amount > 0) {
		this->entity_population_provision[id] = amount;
	}
}

std::optional<int64_t> GameState::get_entity_population_provision(entity_id_t id) const {
	auto it = this->entity_population_provision.find(id);
	if (it == this->entity_population_provision.end()) {
		return std::nullopt;
	}
	return it->second;
}

void GameState::set_entity_max_hp(entity_id_t id, int64_t max_hp) {
	if (max_hp > 0) {
		this->entity_max_hp[id] = max_hp;
	}
}

int64_t GameState::get_entity_max_hp(entity_id_t id) const {
	auto it = this->entity_max_hp.find(id);
	if (it != this->entity_max_hp.end()) {
		return it->second;
	}

	auto entity_it = this->game_entities.find(id);
	if (entity_it != this->game_entities.end()) {
		const auto &entity = entity_it->second;
		if (entity->has_component(component::component_t::LIVE)) {
			auto live = std::dynamic_pointer_cast<component::Live>(
				entity->get_component(component::component_t::LIVE));
			int64_t live_hp = live->get_attribute(time::TIME_MIN, "engine.ability.type.Live.AttributeAmount");
			if (live_hp > 0) {
				return live_hp;
			}
		}

		if (entity->has_component(component::component_t::MOVE)) {
			return DEFAULT_UNIT_MAX_HP;
		}
		else {
			return DEFAULT_BUILDING_MAX_HP;
		}
	}

	return DEFAULT_BUILDING_MAX_HP;
}

bool GameState::has_entity_max_hp(entity_id_t id) const {
	return this->entity_max_hp.contains(id);
}

void GameState::clear_entity_max_hp(entity_id_t id) {
	this->entity_max_hp.erase(id);
}

void GameState::set_entity_armor(entity_id_t id, armor_class_t armor_class, int64_t armor_value) {
	this->entity_armors[id][armor_class] = armor_value;
}

int64_t GameState::get_entity_armor(entity_id_t id, armor_class_t armor_class) const {
	auto it = this->entity_armors.find(id);
	if (it != this->entity_armors.end()) {
		auto class_it = it->second.find(armor_class);
		if (class_it != it->second.end()) {
			return class_it->second;
		}
	}
	if (armor_class == armor_class_t::MELEE or armor_class == armor_class_t::PIERCE) {
		return 0;
	}
	return DEFAULT_UNPOSSESSED_ARMOR;
}

bool GameState::has_entity_armor(entity_id_t id, armor_class_t armor_class) const {
	auto it = this->entity_armors.find(id);
	if (it != this->entity_armors.end()) {
		return it->second.contains(armor_class);
	}
	return false;
}

void GameState::set_entity_attack_bonus(entity_id_t id, armor_class_t armor_class, int64_t bonus) {
	if (bonus != 0) {
		this->entity_attack_bonuses[id][armor_class] = bonus;
	}
	else {
		auto it = this->entity_attack_bonuses.find(id);
		if (it != this->entity_attack_bonuses.end()) {
			it->second.erase(armor_class);
			if (it->second.empty()) {
				this->entity_attack_bonuses.erase(it);
			}
		}
	}
}

int64_t GameState::get_entity_attack_bonus(entity_id_t id, armor_class_t armor_class) const {
	auto it = this->entity_attack_bonuses.find(id);
	if (it != this->entity_attack_bonuses.end()) {
		auto class_it = it->second.find(armor_class);
		if (class_it != it->second.end()) {
			return class_it->second;
		}
	}
	return 0;
}

std::vector<AttackBonus> GameState::get_entity_attack_bonuses(entity_id_t id) const {
	std::vector<AttackBonus> bonuses;
	auto it = this->entity_attack_bonuses.find(id);
	if (it != this->entity_attack_bonuses.end()) {
		bonuses.reserve(it->second.size());
		for (const auto &[cls, bonus] : it->second) {
			bonuses.push_back(AttackBonus{cls, bonus});
		}
	}
	return bonuses;
}

void GameState::clear_entity_combat_stats(entity_id_t id) {
	this->entity_armors.erase(id);
	this->entity_attack_bonuses.erase(id);
}

void GameState::set_garrison_capacity(entity_id_t building_id, int64_t capacity) {
	if (capacity > 0) {
		this->garrison_capacities[building_id] = capacity;
	}
}

int64_t GameState::get_garrison_capacity(entity_id_t building_id) const {
	auto it = this->garrison_capacities.find(building_id);
	if (it != this->garrison_capacities.end()) {
		return it->second;
	}

	auto entity_it = this->game_entities.find(building_id);
	if (entity_it != this->game_entities.end()) {
		const auto &entity = entity_it->second;
		if (entity->has_component(component::component_t::MOVE)) {
			return GARRISON_CAPACITY_RAM;
		}
		if (entity->has_component(component::component_t::ATTACK)) {
			return GARRISON_CAPACITY_TOWER;
		}
		return GARRISON_CAPACITY_TOWN_CENTER;
	}
	return GARRISON_CAPACITY_TOWN_CENTER;
}

bool GameState::can_garrison(entity_id_t unit_id, entity_id_t building_id) const {
	if (unit_id == building_id) {
		return false;
	}
	if (this->is_garrisoned(unit_id)) {
		return false;
	}
	auto unit_it = this->game_entities.find(unit_id);
	auto b_it = this->game_entities.find(building_id);
	if (unit_it == this->game_entities.end() or b_it == this->game_entities.end()) {
		return false;
	}

	const auto &unit = unit_it->second;
	const auto &building = b_it->second;

	// Must be owned by same player or friendly
	if (unit->has_component(component::component_t::OWNERSHIP)
	    and building->has_component(component::component_t::OWNERSHIP)) {
		auto u_own = std::dynamic_pointer_cast<component::Ownership>(
			unit->get_component(component::component_t::OWNERSHIP));
		auto b_own = std::dynamic_pointer_cast<component::Ownership>(
			building->get_component(component::component_t::OWNERSHIP));
		if (u_own->get_owners().get(time::TIME_MIN) != b_own->get_owners().get(time::TIME_MIN)) {
			return false;
		}
	}

	auto garr_it = this->building_garrisons.find(building_id);
	int64_t current_count = (garr_it != this->building_garrisons.end())
	                        ? static_cast<int64_t>(garr_it->second.size())
	                        : 0;
	return current_count < this->get_garrison_capacity(building_id);
}

bool GameState::garrison_entity(entity_id_t unit_id, entity_id_t building_id, const time::time_t &time) {
	if (not this->can_garrison(unit_id, building_id)) {
		return false;
	}

	auto unit_it = this->game_entities.find(unit_id);
	auto b_it = this->game_entities.find(building_id);
	if (unit_it == this->game_entities.end() or b_it == this->game_entities.end()) {
		return false;
	}

	const auto &unit = unit_it->second;
	const auto &building = b_it->second;

	// Release tile occupancy while garrisoned
	this->release_tile(unit_id);

	// Update unit position to building position
	if (building->has_component(component::component_t::POSITION)
	    and unit->has_component(component::component_t::POSITION)) {
		auto b_pos_comp = std::dynamic_pointer_cast<component::Position>(
			building->get_component(component::component_t::POSITION));
		auto u_pos_comp = std::dynamic_pointer_cast<component::Position>(
			unit->get_component(component::component_t::POSITION));
		u_pos_comp->set_position(time, b_pos_comp->get_positions().get(time));
	}

	this->building_garrisons[building_id].push_back(unit_id);
	this->unit_garrison_parent[unit_id] = building_id;

	log::log(MSG(info) << "Unit " << unit_id << " garrisoned in building " << building_id << ".");
	return true;
}

std::vector<entity_id_t> GameState::ungarrison_entities(entity_id_t building_id, const time::time_t &time) {
	std::vector<entity_id_t> ejected;
	auto it = this->building_garrisons.find(building_id);
	if (it == this->building_garrisons.end() or it->second.empty()) {
		return ejected;
	}

	ejected = std::move(it->second);
	this->building_garrisons.erase(it);

	coord::phys3 exit_pos{0, 0, 0};
	bool has_exit_pos = false;

	if (this->has_rally_point(building_id)) {
		exit_pos = this->get_rally_point(building_id).value();
		has_exit_pos = true;
	}
	else {
		auto b_it = this->game_entities.find(building_id);
		if (b_it != this->game_entities.end() and b_it->second->has_component(component::component_t::POSITION)) {
			auto b_pos_comp = std::dynamic_pointer_cast<component::Position>(
				b_it->second->get_component(component::component_t::POSITION));
			auto b_pos = b_pos_comp->get_positions().get(time);
			exit_pos = b_pos + coord::phys3_delta{coord::phys_t{2.0}, coord::phys_t{0.0}, coord::phys_t{0.0}};
			has_exit_pos = true;
		}
	}

	for (entity_id_t unit_id : ejected) {
		this->unit_garrison_parent.erase(unit_id);

		auto u_it = this->game_entities.find(unit_id);
		if (u_it != this->game_entities.end()) {
			const auto &unit = u_it->second;
			if (has_exit_pos and unit->has_component(component::component_t::POSITION)) {
				auto u_pos_comp = std::dynamic_pointer_cast<component::Position>(
					unit->get_component(component::component_t::POSITION));
				u_pos_comp->set_position(time, exit_pos);
				this->occupy_tile(unit_id, exit_pos);
			}

			// Restore saved task if unit was garrisoned via Town Bell
			auto task_it = this->saved_tasks.find(unit_id);
			if (task_it != this->saved_tasks.end() and unit->has_component(component::component_t::COMMANDQUEUE)) {
				auto cmd_q = std::dynamic_pointer_cast<component::CommandQueue>(
					unit->get_component(component::component_t::COMMANDQUEUE));
				cmd_q->add_command(time, task_it->second);
				this->saved_tasks.erase(task_it);
			}
		}
	}

	log::log(MSG(info) << "Ungarrisoned " << ejected.size() << " units from building " << building_id << ".");
	return ejected;
}

bool GameState::is_garrisoned(entity_id_t unit_id) const {
	return this->unit_garrison_parent.contains(unit_id);
}

std::optional<entity_id_t> GameState::get_garrison_parent(entity_id_t unit_id) const {
	auto it = this->unit_garrison_parent.find(unit_id);
	if (it != this->unit_garrison_parent.end()) {
		return it->second;
	}
	return std::nullopt;
}

std::vector<entity_id_t> GameState::get_garrisoned_units(entity_id_t building_id) const {
	auto it = this->building_garrisons.find(building_id);
	if (it != this->building_garrisons.end()) {
		return it->second;
	}
	return {};
}

int64_t GameState::get_building_additional_arrows(entity_id_t building_id, const time::time_t & /* time */) const {
	auto it = this->building_garrisons.find(building_id);
	if (it == this->building_garrisons.end() or it->second.empty()) {
		return 0;
	}

	double building_dps = 2.5;
	auto b_it = this->game_entities.find(building_id);
	if (b_it != this->game_entities.end() and b_it->second->has_component(component::component_t::ATTACK)) {
		auto b_atk = std::dynamic_pointer_cast<component::Attack>(
			b_it->second->get_component(component::component_t::ATTACK));
		auto reload = b_atk->get_reload_time();
		auto dmg = b_atk->get_damage();
		if (reload and reload->get() > 0.0 and dmg) {
			building_dps = static_cast<double>(dmg->get()) / reload->get();
		}
	}

	if (building_dps <= 0.0) {
		building_dps = 2.5;
	}

	double sum_unit_dps = 0.0;
	for (entity_id_t unit_id : it->second) {
		auto u_it = this->game_entities.find(unit_id);
		if (u_it == this->game_entities.end()) {
			continue;
		}
		const auto &unit = u_it->second;
		if (unit->has_component(component::component_t::ATTACK)) {
			auto u_atk = std::dynamic_pointer_cast<component::Attack>(
				unit->get_component(component::component_t::ATTACK));
			auto reload = u_atk->get_reload_time();
			auto dmg = u_atk->get_damage();
			if (reload and reload->get() > 0.0 and dmg) {
				sum_unit_dps += static_cast<double>(dmg->get()) / reload->get();
			}
			else {
				sum_unit_dps += 2.5;
			}
		}
		else {
			// Villagers contribute 2.5 pierce DPS by AoE2 rules
			sum_unit_dps += 2.5;
		}
	}

	return static_cast<int64_t>(std::floor(sum_unit_dps / building_dps));
}

void GameState::ring_town_bell(entity_id_t tc_id, const time::time_t &time) {
	auto tc_it = this->game_entities.find(tc_id);
	if (tc_it == this->game_entities.end() or not tc_it->second->has_component(component::component_t::POSITION)) {
		return;
	}

	player_id_t owner_id = 0;
	if (tc_it->second->has_component(component::component_t::OWNERSHIP)) {
		auto tc_own = std::dynamic_pointer_cast<component::Ownership>(
			tc_it->second->get_component(component::component_t::OWNERSHIP));
		owner_id = tc_own->get_owners().get(time);
	}

	auto tc_pos_comp = std::dynamic_pointer_cast<component::Position>(
		tc_it->second->get_component(component::component_t::POSITION));
	auto tc_pos = tc_pos_comp->get_positions().get(time);

	std::vector<entity_id_t> candidate_garrisons;
	for (const auto &[b_id, entity] : this->game_entities) {
		if (entity->has_component(component::component_t::MOVE)
		    or not entity->has_component(component::component_t::POSITION)
		    or not entity->has_component(component::component_t::OWNERSHIP)) {
			continue;
		}
		auto own = std::dynamic_pointer_cast<component::Ownership>(
			entity->get_component(component::component_t::OWNERSHIP));
		if (own->get_owners().get(time) != owner_id) {
			continue;
		}
		auto pos_comp = std::dynamic_pointer_cast<component::Position>(
			entity->get_component(component::component_t::POSITION));
		double d = (pos_comp->get_positions().get(time) - tc_pos).length();
		if (d <= TOWN_BELL_RANGE_TILES) {
			candidate_garrisons.push_back(b_id);
		}
	}

	if (std::find(candidate_garrisons.begin(), candidate_garrisons.end(), tc_id) == candidate_garrisons.end()) {
		candidate_garrisons.push_back(tc_id);
	}

	for (const auto &[u_id, entity] : this->game_entities) {
		if (u_id == tc_id
		    or not entity->has_component(component::component_t::MOVE)
		    or not entity->has_component(component::component_t::POSITION)
		    or not entity->has_component(component::component_t::OWNERSHIP)
		    or not entity->has_component(component::component_t::COMMANDQUEUE)
		    or this->is_garrisoned(u_id)) {
			continue;
		}
		auto own = std::dynamic_pointer_cast<component::Ownership>(
			entity->get_component(component::component_t::OWNERSHIP));
		if (own->get_owners().get(time) != owner_id) {
			continue;
		}

		auto pos_comp = std::dynamic_pointer_cast<component::Position>(
			entity->get_component(component::component_t::POSITION));
		auto u_pos = pos_comp->get_positions().get(time);
		double dist_to_tc = (u_pos - tc_pos).length();
		if (dist_to_tc > TOWN_BELL_RANGE_TILES) {
			continue;
		}

		entity_id_t best_garrison = 0;
		double best_dist = std::numeric_limits<double>::max();
		for (entity_id_t g_id : candidate_garrisons) {
			if (not this->can_garrison(u_id, g_id)) {
				continue;
			}
			auto g_it = this->game_entities.find(g_id);
			if (g_it == this->game_entities.end()) {
				continue;
			}
			auto g_pos_comp = std::dynamic_pointer_cast<component::Position>(
				g_it->second->get_component(component::component_t::POSITION));
			double d = (g_pos_comp->get_positions().get(time) - u_pos).length();
			if (d <= TOWN_BELL_RANGE_TILES and d < best_dist) {
				best_dist = d;
				best_garrison = g_id;
			}
		}

		if (best_garrison != 0) {
			auto cmd_q = std::dynamic_pointer_cast<component::CommandQueue>(
				entity->get_component(component::component_t::COMMANDQUEUE));
			if (not cmd_q->get_queue().empty(time)) {
				this->saved_tasks[u_id] = cmd_q->get_queue().front(time);
				cmd_q->pop_command(time);
			}
			cmd_q->add_command(
				time,
				std::make_shared<component::command::GarrisonCommand>(best_garrison));
		}
	}
}

void GameState::back_to_work(entity_id_t tc_id, const time::time_t &time) {
	auto tc_it = this->game_entities.find(tc_id);
	if (tc_it == this->game_entities.end() or not tc_it->second->has_component(component::component_t::POSITION)) {
		return;
	}

	auto tc_pos_comp = std::dynamic_pointer_cast<component::Position>(
		tc_it->second->get_component(component::component_t::POSITION));
	auto tc_pos = tc_pos_comp->get_positions().get(time);

	std::vector<entity_id_t> garrisons_to_empty;
	for (const auto &[b_id, units] : this->building_garrisons) {
		(void) units;
		auto b_it = this->game_entities.find(b_id);
		if (b_it != this->game_entities.end() and b_it->second->has_component(component::component_t::POSITION)) {
			auto b_pos = std::dynamic_pointer_cast<component::Position>(
				b_it->second->get_component(component::component_t::POSITION))->get_positions().get(time);
			if ((b_pos - tc_pos).length() <= TOWN_BELL_RANGE_TILES) {
				garrisons_to_empty.push_back(b_id);
			}
		}
	}

	for (entity_id_t g_id : garrisons_to_empty) {
		this->ungarrison_entities(g_id, time);
	}
}

void GameState::tick_garrison_heal(const time::time_t &time, double dt_sec) {
	if (dt_sec <= 0.0) {
		return;
	}
	int64_t hp_heal = static_cast<int64_t>(std::ceil(GARRISON_HEAL_HP_PER_SEC * dt_sec));
	if (hp_heal <= 0) {
		hp_heal = 1;
	}

	for (const auto &[b_id, units] : this->building_garrisons) {
		(void) b_id;
		for (entity_id_t unit_id : units) {
			auto u_it = this->game_entities.find(unit_id);
			if (u_it != this->game_entities.end() and u_it->second->has_component(component::component_t::LIVE)) {
				auto live = std::dynamic_pointer_cast<component::Live>(
					u_it->second->get_component(component::component_t::LIVE));
				constexpr const char *HP_ATTR = "engine.ability.type.Live.AttributeAmount";
				int64_t cur_hp = live->get_attribute(time, HP_ATTR);
				int64_t max_hp = this->get_entity_max_hp(unit_id);
				if (cur_hp < max_hp) {
					int64_t next_hp = std::min(cur_hp + hp_heal, max_hp);
					live->set_attribute(time, HP_ATTR, next_hp);
				}
			}
		}
	}
}

entity_id_t GameState::allocate_entity_id() const {
	entity_id_t max_id = 0;
	for (const auto &[id, entity] : this->game_entities) {
		(void) entity;
		if (id > max_id) {
			max_id = id;
		}
	}
	return max_id + 1;
}

void GameState::spawn_salvage_pile(const coord::phys3 &position,
                                   const BuildingCostRecord &cost,
                                   double recovery_fraction,
                                   const time::time_t &time) {
	const double fraction = clamp_recovery_fraction(recovery_fraction);
	if (fraction <= 0 || cost.empty()) {
		return;
	}

	size_t pile_index = 0;
	for (const auto &entry : cost.entries) {
		if (entry.amount <= 0 || entry.resource_type.empty()) {
			continue;
		}

		int64_t salvage_amount = static_cast<int64_t>(std::floor(entry.amount * fraction));
		if (salvage_amount <= 0) {
			continue;
		}

		coord::phys3 pile_pos = position;
		if (pile_index > 0) {
			pile_pos.ne += static_cast<coord::phys_t>(pile_index) * 0.5;
		}

		auto entity_id = this->allocate_entity_id();
		auto entity = std::make_shared<GameEntity>(entity_id);

		auto position_comp = std::make_shared<component::Position>(this->event_loop);
		position_comp->set_position(time, pile_pos);
		entity->add_component(position_comp);

		auto salvage_comp = std::make_shared<component::Salvage>(
			this->event_loop, entry.resource_type, salvage_amount, time);
		entity->add_component(salvage_comp);

		this->add_game_entity(entity);

		log::log(MSG(info) << "Spawned salvage pile " << entity_id
		                   << " with " << salvage_amount << " of " << entry.resource_type
		                   << " at " << pile_pos << ".");

		++pile_index;
	}
}

void GameState::tick_salvage_decay(const time::time_t &time) {
	std::vector<entity_id_t> depleted;

	for (entity_id_t id : this->salvage_pile_ids) {
		auto it = this->game_entities.find(id);
		if (it == this->game_entities.end()) {
			depleted.push_back(id);
			continue;
		}

		if (not it->second->has_component(component::component_t::SALVAGE)) {
			depleted.push_back(id);
			continue;
		}

		auto salvage = std::dynamic_pointer_cast<component::Salvage>(
			it->second->get_component(component::component_t::SALVAGE));

		int64_t current = salvage->get_amount(time);
		if (current <= 0) {
			depleted.push_back(id);
			continue;
		}

		auto last_decay = salvage->get_last_decay_time();
		double elapsed = time.to_double() - last_decay.to_double();
		if (elapsed < SALVAGE_DECAY_INTERVAL_SEC) {
			continue;
		}

		int64_t decay_units = static_cast<int64_t>(elapsed / SALVAGE_DECAY_INTERVAL_SEC);
		int64_t new_amount = std::max(int64_t{0}, current - decay_units);
		salvage->set_amount(time, new_amount);
		salvage->set_last_decay_time(time);

		if (new_amount == 0) {
			depleted.push_back(id);
		}
	}

	for (entity_id_t id : depleted) {
		this->remove_game_entity(id);
	}
}

void GameState::set_forest_regen_enabled(bool enabled) {
	this->forest_regen_enabled = enabled;
}

bool GameState::is_forest_regen_enabled() const {
	return this->forest_regen_enabled;
}

void GameState::set_forest_regen_params(double interval_sec, int64_t amount) {
	if (interval_sec > 0) {
		this->forest_regen_interval_sec = interval_sec;
	}
	if (amount > 0) {
		this->forest_regen_amount = amount;
	}
}

void GameState::register_resource_node(entity_id_t id,
                                       int64_t max_amount,
                                       const time::time_t &time) {
	auto it = this->resource_nodes.find(id);
	if (it == this->resource_nodes.end()) {
		this->resource_nodes.emplace(id, ResourceNodeState{max_amount, time});
	}
	else if (max_amount > it->second.max_amount) {
		// Record the largest amount the node has been seen holding.
		it->second.max_amount = max_amount;
	}
}

bool GameState::is_resource_node(entity_id_t id) const {
	return this->resource_nodes.contains(id);
}

void GameState::tick_resource_regen(const time::time_t &time) {
	if (not this->forest_regen_enabled) {
		return;
	}

	// nyan attribute holding a resource entity's current amount (shared with Gather).
	static constexpr const char *RESOURCE_AMOUNT_ATTRIBUTE =
		"engine.ability.type.Live.AttributeAmount";

	std::vector<entity_id_t> stale;

	for (auto &[id, node] : this->resource_nodes) {
		auto it = this->game_entities.find(id);
		if (it == this->game_entities.end()
		    || not it->second->has_component(component::component_t::LIVE)) {
			stale.push_back(id);
			continue;
		}

		double elapsed = time.to_double() - node.last_regen_time.to_double();
		if (elapsed < this->forest_regen_interval_sec) {
			continue;
		}

		auto live = std::dynamic_pointer_cast<component::Live>(
			it->second->get_component(component::component_t::LIVE));
		int64_t current = live->get_attribute(time, RESOURCE_AMOUNT_ATTRIBUTE);

		if (current >= node.max_amount) {
			// Already full; just advance the regen clock.
			node.last_regen_time = time;
			continue;
		}

		int64_t steps = static_cast<int64_t>(elapsed / this->forest_regen_interval_sec);
		int64_t regenerated = std::min(node.max_amount,
		                               current + steps * this->forest_regen_amount);
		live->set_attribute(time, RESOURCE_AMOUNT_ATTRIBUTE, regenerated);
		node.last_regen_time = time;

		log::log(MSG(dbg) << "Resource node " << id << " regenerated to "
		                  << regenerated << " / " << node.max_amount << ".");
	}

	for (entity_id_t id : stale) {
		this->resource_nodes.erase(id);
	}
}

void GameState::set_streets_enabled(bool enabled) {
	this->streets_enabled = enabled;
}

bool GameState::is_streets_enabled() const {
	return this->streets_enabled;
}

void GameState::set_street_move_mult(double mult) {
	if (mult > 0) {
		this->street_move_mult = mult;
	}
}

void GameState::register_street_tile(coord::tile tile, entity_id_t building_id) {
	this->unregister_street_by_entity(building_id);
	// Evict any previous street owner of this tile so destroy cleanup stays consistent.
	for (auto it = this->entity_street_tile.begin(); it != this->entity_street_tile.end();) {
		if (it->second == tile) {
			it = this->entity_street_tile.erase(it);
		}
		else {
			++it;
		}
	}
	this->street_tiles.insert(tile);
	this->entity_street_tile.insert_or_assign(building_id, tile);
}

void GameState::unregister_street_tile(coord::tile tile) {
	this->street_tiles.erase(tile);
	for (auto it = this->entity_street_tile.begin(); it != this->entity_street_tile.end();) {
		if (it->second == tile) {
			it = this->entity_street_tile.erase(it);
		}
		else {
			++it;
		}
	}
}

void GameState::unregister_street_by_entity(entity_id_t building_id) {
	auto it = this->entity_street_tile.find(building_id);
	if (it == this->entity_street_tile.end()) {
		return;
	}
	this->street_tiles.erase(it->second);
	this->entity_street_tile.erase(it);
}

bool GameState::is_street_tile(coord::tile tile) const {
	return this->street_tiles.contains(tile);
}

bool GameState::can_place_street(coord::tile tile) const {
	if (not this->streets_enabled) {
		return false;
	}
	if (this->is_street_tile(tile) or this->is_bridge_tile(tile)) {
		return false;
	}
	if (this->is_tile_occupied(tile)) {
		return false;
	}
	return this->is_land_tile(tile);
}

void GameState::set_bridges_enabled(bool enabled) {
	this->bridges_enabled = enabled;
}

bool GameState::is_bridges_enabled() const {
	return this->bridges_enabled;
}

void GameState::register_bridge_tile(coord::tile tile, entity_id_t building_id) {
	this->unregister_bridge_by_entity(building_id);
	// Evict any previous bridge owner of this tile.
	auto existing = this->bridge_tiles.find(tile);
	if (existing != this->bridge_tiles.end() and existing->second != building_id) {
		this->entity_bridge_tile.erase(existing->second);
	}
	this->bridge_tiles.insert_or_assign(tile, building_id);
	this->entity_bridge_tile.insert_or_assign(building_id, tile);
}

void GameState::unregister_bridge_tile(coord::tile tile) {
	auto it = this->bridge_tiles.find(tile);
	if (it == this->bridge_tiles.end()) {
		return;
	}
	this->entity_bridge_tile.erase(it->second);
	this->bridge_tiles.erase(it);
}

void GameState::unregister_bridge_by_entity(entity_id_t building_id) {
	auto it = this->entity_bridge_tile.find(building_id);
	if (it == this->entity_bridge_tile.end()) {
		return;
	}
	this->bridge_tiles.erase(it->second);
	this->entity_bridge_tile.erase(it);
}

bool GameState::is_bridge_tile(coord::tile tile) const {
	return this->bridge_tiles.contains(tile);
}

bool GameState::can_place_bridge(coord::tile tile) const {
	if (not this->bridges_enabled) {
		return false;
	}
	if (this->is_bridge_tile(tile) or this->is_street_tile(tile)) {
		return false;
	}
	if (this->is_tile_occupied(tile)) {
		return false;
	}
	// Without Water path grids (unit tests), allow placement so lifecycle
	// tests can exercise registration without a full nyan PathType setup.
	if (this->map == nullptr) {
		return true;
	}
	auto water = this->map->find_grid_by_suffix("Water");
	if (not water.has_value()) {
		return true;
	}
	return this->is_water_tile(tile);
}

bool GameState::is_land_tile(coord::tile tile) const {
	if (this->map == nullptr) {
		return true;
	}
	auto land = this->map->find_grid_by_suffix("Land");
	if (not land.has_value()) {
		return true;
	}
	auto cost = this->map->get_tile_cost(land.value(), tile);
	if (not cost.has_value()) {
		return false;
	}
	return cost.value() != path::COST_IMPASSABLE;
}

bool GameState::is_water_tile(coord::tile tile) const {
	if (this->map == nullptr) {
		return false;
	}
	auto water = this->map->find_grid_by_suffix("Water");
	if (not water.has_value()) {
		return false;
	}
	auto water_cost = this->map->get_tile_cost(water.value(), tile);
	if (not water_cost.has_value() or water_cost.value() == path::COST_IMPASSABLE) {
		return false;
	}
	auto land = this->map->find_grid_by_suffix("Land");
	if (land.has_value()) {
		auto land_cost = this->map->get_tile_cost(land.value(), tile);
		if (land_cost.has_value() and land_cost.value() != path::COST_IMPASSABLE) {
			return false;
		}
	}
	return true;
}

double GameState::get_tile_move_speed_multiplier(coord::tile tile) const {
	if (this->streets_enabled and this->is_street_tile(tile)) {
		return this->street_move_mult;
	}
	return 1.0;
}

void GameState::apply_bridge_path_costs(path::grid_id_t grid_id, const time::time_t &time) {
	if (not this->bridges_enabled or this->bridge_tiles.empty() or this->map == nullptr) {
		return;
	}

	const auto kind = this->map->classify_grid(grid_id);
	if (kind == path_grid_kind_t::OTHER) {
		return;
	}

	const path::cost_t cost = (kind == path_grid_kind_t::LAND)
	                              ? path::COST_MIN
	                              : path::COST_IMPASSABLE;

	for (const auto &[tile, building_id] : this->bridge_tiles) {
		(void) building_id;
		this->map->set_tile_cost(grid_id, tile, cost, time);
	}
}

void GameState::set_day_night_enabled(bool enabled) {
	this->day_night_enabled = enabled;
}

bool GameState::is_day_night_enabled() const {
	return this->day_night_enabled;
}

void GameState::set_day_night_params(double day_sec, double night_sec) {
	if (day_sec > 0) {
		this->day_length_sec = day_sec;
	}
	if (night_sec > 0) {
		this->night_length_sec = night_sec;
	}
}

day_phase_t GameState::get_day_phase(const time::time_t &time) const {
	if (not this->day_night_enabled) {
		return day_phase_t::DAY;
	}

	const double cycle = this->day_length_sec + this->night_length_sec;
	if (cycle <= 0) {
		return day_phase_t::DAY;
	}

	double t = std::fmod(time.to_double(), cycle);
	if (t < 0) {
		t += cycle;
	}

	const double dusk_start = this->day_length_sec * (1.0 - TWILIGHT_FRACTION);
	const double night_start = this->day_length_sec;
	const double dawn_start = this->day_length_sec
	                          + this->night_length_sec * (1.0 - TWILIGHT_FRACTION);

	if (t < dusk_start) {
		return day_phase_t::DAY;
	}
	if (t < night_start) {
		return day_phase_t::DUSK;
	}
	if (t < dawn_start) {
		return day_phase_t::NIGHT;
	}
	return day_phase_t::DAWN;
}

void GameState::set_weather_enabled(bool enabled) {
	this->weather_enabled = enabled;
	if (not enabled) {
		this->current_weather = weather_t::CLEAR;
	}
}

bool GameState::is_weather_enabled() const {
	return this->weather_enabled;
}

void GameState::set_weather(weather_t weather) {
	this->current_weather = weather;
}

weather_t GameState::get_weather() const {
	if (not this->weather_enabled) {
		return weather_t::CLEAR;
	}
	return this->current_weather;
}

void GameState::tick_environment(const time::time_t &time) {
	if (not this->weather_enabled) {
		return;
	}

	double elapsed = time.to_double() - this->last_weather_change_time.to_double();
	if (elapsed < WEATHER_CYCLE_INTERVAL_SEC) {
		return;
	}

	// Cycle CLEAR -> FOG -> RAIN -> CLEAR.
	switch (this->current_weather) {
	case weather_t::CLEAR:
		this->current_weather = weather_t::FOG;
		break;
	case weather_t::FOG:
		this->current_weather = weather_t::RAIN;
		break;
	case weather_t::RAIN:
		this->current_weather = weather_t::CLEAR;
		break;
	}
	this->last_weather_change_time = time;
}

double GameState::get_sight_multiplier(const time::time_t &time) const {
	double mult = 1.0;

	if (this->day_night_enabled) {
		switch (this->get_day_phase(time)) {
		case day_phase_t::DAY:
			mult *= DAY_SIGHT_MULT;
			break;
		case day_phase_t::DUSK:
		case day_phase_t::DAWN:
			mult *= TWILIGHT_SIGHT_MULT;
			break;
		case day_phase_t::NIGHT:
			mult *= NIGHT_SIGHT_MULT;
			break;
		}
	}

	if (this->weather_enabled) {
		switch (this->current_weather) {
		case weather_t::CLEAR:
			mult *= WEATHER_CLEAR_SIGHT_MULT;
			break;
		case weather_t::FOG:
			mult *= WEATHER_FOG_SIGHT_MULT;
			break;
		case weather_t::RAIN:
			mult *= WEATHER_RAIN_SIGHT_MULT;
			break;
		}
	}

	return mult;
}

double GameState::get_move_speed_multiplier() const {
	if (not this->weather_enabled) {
		return WEATHER_CLEAR_MOVE_MULT;
	}

	switch (this->current_weather) {
	case weather_t::CLEAR:
		return WEATHER_CLEAR_MOVE_MULT;
	case weather_t::FOG:
		return WEATHER_FOG_MOVE_MULT;
	case weather_t::RAIN:
		return WEATHER_RAIN_MOVE_MULT;
	}
	return WEATHER_CLEAR_MOVE_MULT;
}

void GameState::set_forest_hide_enabled(bool enabled) {
	this->forest_hide_enabled = enabled;
}

bool GameState::is_forest_hide_enabled() const {
	return this->forest_hide_enabled;
}

void GameState::set_forest_hide_threshold(int tiles) {
	if (tiles >= 0) {
		this->forest_hide_threshold = tiles;
	}
}

int GameState::get_forest_hide_threshold() const {
	return this->forest_hide_threshold;
}

void GameState::mark_forest_tile(coord::tile tile) {
	this->forest_tiles.insert(tile);
}

void GameState::unmark_forest_tile(coord::tile tile) {
	this->forest_tiles.erase(tile);
}

bool GameState::is_forest_tile(coord::tile tile) const {
	return this->forest_tiles.contains(tile);
}

void GameState::clear_forest_tiles() {
	this->forest_tiles.clear();
}

namespace {

bool name_contains_forest(const std::string &name) {
	std::string lower;
	lower.reserve(name.size());
	for (char c : name) {
		lower.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
	}
	return lower.find("forest") != std::string::npos;
}

} // namespace

void GameState::rebuild_forest_tiles_from_terrain() {
	this->forest_tiles.clear();
	if (this->map == nullptr) {
		return;
	}

	const auto &terrain = this->map->get_terrain();
	if (terrain == nullptr) {
		return;
	}

	for (const auto &chunk : terrain->get_chunks()) {
		const auto &offset = chunk->get_offset();
		const auto &size = chunk->get_size();
		const auto &tiles = chunk->get_tiles();
		if (tiles.empty()) {
			continue;
		}

		for (size_t se = 0; se < size[1]; ++se) {
			for (size_t ne = 0; ne < size[0]; ++ne) {
				const size_t idx = ne + se * size[0];
				if (idx >= tiles.size()) {
					continue;
				}
				const auto &tile = tiles[idx];
				// Match terrain asset paths / fqons that mention "forest"
				// (e.g. aoe1_base.data.terrain.forest.forest.Forest).
				if (name_contains_forest(tile.terrain_asset_path)
				    || name_contains_forest(tile.terrain_fqon)) {
					coord::tile world{
						offset.ne + static_cast<coord::tile_t>(ne),
						offset.se + static_cast<coord::tile_t>(se)};
					this->forest_tiles.insert(world);
				}
			}
		}
	}
}


void GameState::finish_deconstruct(entity_id_t building_id, const time::time_t &time) {
	if (not this->game_entities.contains(building_id)) {
		return;
	}
	// Cost was cleared when deconstruction started so destroy salvage is not spawned.
	this->remove_game_entity(building_id, time);
}

void GameState::complete_deconstruction(entity_id_t building_id,
                                        const coord::phys3 &position,
                                        const BuildingCostRecord &cost,
                                        double recovery_fraction,
                                        const time::time_t &time) {
	this->spawn_salvage_pile(position, cost, recovery_fraction, time);

	if (this->game_entities.contains(building_id)) {
		this->finish_deconstruct(building_id, time);
	}
	else {
		log::log(MSG(dbg) << "Deconstruction salvage spawned at " << position
		                  << "; building " << building_id << " was already removed.");
	}
}

const std::shared_ptr<assets::ModManager> &GameState::get_mod_manager() const {
	return this->mod_manager;
}

void GameState::set_mod_manager(const std::shared_ptr<assets::ModManager> &mod_manager) {
	this->mod_manager = mod_manager;
}

namespace {

constexpr const char *HP_ATTRIBUTE = "engine.ability.type.Live.AttributeAmount";
constexpr int DEFAULT_SIGHT_RANGE_TILES = 4;

bool entity_provides_visibility(const std::shared_ptr<GameEntity> &entity,
                                const time::time_t &time) {
	if (not entity->has_component(component::component_t::POSITION)
	    || not entity->has_component(component::component_t::OWNERSHIP)) {
		return false;
	}

	if (entity->has_component(component::component_t::LIVE)) {
		auto live = std::dynamic_pointer_cast<component::Live>(
			entity->get_component(component::component_t::LIVE));
		if (live->get_attribute(time, HP_ATTRIBUTE) <= 0) {
			return false;
		}
	}

	return true;
}

} // namespace

void GameState::refresh_visibility(const time::time_t &time) {
	for (const auto &[player_id, player] : this->players) {
		(void) player;
		this->flush_player_visibility(player_id);
	}

	for (const auto &[entity_id, entity] : this->game_entities) {
		if (not entity_provides_visibility(entity, time)) {
			continue;
		}

		auto ownership = std::dynamic_pointer_cast<component::Ownership>(
			entity->get_component(component::component_t::OWNERSHIP));
		auto owner_id = ownership->get_owners().get(time);
		if (not this->has_player(owner_id)) {
			continue;
		}

		auto pos_comp = std::dynamic_pointer_cast<component::Position>(
			entity->get_component(component::component_t::POSITION));
		auto center = pos_comp->get_positions().get(time).to_tile();

		double sight_mult = this->get_sight_multiplier(time);
		int sight_range = static_cast<int>(
			std::lround(DEFAULT_SIGHT_RANGE_TILES * sight_mult));
		if (sight_range < 0) {
			sight_range = 0;
		}

		this->update_player_visibility(owner_id, center, sight_range);
	}

	this->update_fog_render_visibility(time);
	this->update_fog_tile_texture();
	this->update_minimap_texture(time);
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
	auto pos = pos_comp->get_positions().get(time);
	auto tile = pos.to_tile();

	bool visible = this->fog_of_war.is_visible(observer, tile);
	if (not visible) {
		return false;
	}

	// Forest hiding: enemy units on forest tiles are only visible when an
	// observer unit is within the Chebyshev detection threshold.
	if (this->forest_hide_enabled && this->is_forest_tile(tile)) {
		player_id_t owner_id = observer;
		if (entity->has_component(component::component_t::OWNERSHIP)) {
			auto ownership = std::dynamic_pointer_cast<component::Ownership>(
				entity->get_component(component::component_t::OWNERSHIP));
			owner_id = ownership->get_owners().get(time);
		}

		if (owner_id != observer) {
			bool detected = false;
			for (const auto &[ally_id, ally] : this->game_entities) {
				(void) ally_id;
				if (not ally->has_component(component::component_t::POSITION)
				    || not ally->has_component(component::component_t::OWNERSHIP)) {
					continue;
				}
				auto ally_own = std::dynamic_pointer_cast<component::Ownership>(
					ally->get_component(component::component_t::OWNERSHIP));
				if (ally_own->get_owners().get(time) != observer) {
					continue;
				}
				auto ally_pos = std::dynamic_pointer_cast<component::Position>(
					ally->get_component(component::component_t::POSITION));
				auto ally_tile = ally_pos->get_positions().get(time).to_tile();
				auto dne = ally_tile.ne - tile.ne;
				auto dse = ally_tile.se - tile.se;
				if (dne < 0) {
					dne = -dne;
				}
				if (dse < 0) {
					dse = -dse;
				}
				auto chebyshev = std::max(dne, dse);
				if (chebyshev <= static_cast<coord::tile_t>(this->forest_hide_threshold)) {
					detected = true;
					break;
				}
			}
			if (not detected) {
				return false;
			}
		}
	}

	// Remember the entity's position whenever it is visible to the observer.
	// Once it leaves vision we keep that remembered spot untouched so it can
	// be rendered as a "ghost" at the place it was last seen. Entities the
	// observer has never seen have no recorded position and stay hidden.
	// Because is_entity_visible is a const query we use mutable fog_of_war.
	const_cast<FogOfWar &>(this->fog_of_war).set_last_known_position(observer, entity_id, pos);

	return true;
}

std::optional<coord::phys3> GameState::get_last_known_position(player_id_t observer,
                                                               entity_id_t entity_id) const {
	return this->fog_of_war.get_last_known_position(observer, entity_id);
}

void GameState::set_view_player(player_id_t player) {
	this->view_player_id = player;
}

player_id_t GameState::get_view_player() const {
	return this->view_player_id;
}

namespace {

constexpr uint8_t FOG_TEX_UNEXPLORED = 0;
constexpr uint8_t FOG_TEX_EXPLORED = 128;
constexpr uint8_t FOG_TEX_VISIBLE = 255;

} // namespace

void GameState::update_fog_tile_texture() {
	if (this->map == nullptr) {
		return;
	}

	const auto &map_size = this->map->get_size();
	const size_t width = map_size[0];
	const size_t height = map_size[1];
	if (width == 0 || height == 0) {
		return;
	}

	const player_id_t observer = this->view_player_id;
	std::vector<uint8_t> pixels(width * height * 4, 0);

	for (size_t se = 0; se < height; ++se) {
		for (size_t ne = 0; ne < width; ++ne) {
			coord::tile tile{static_cast<coord::tile_t>(ne), static_cast<coord::tile_t>(se)};
			uint8_t value = FOG_TEX_UNEXPLORED;
			if (this->fog_of_war.is_visible(observer, tile)) {
				value = FOG_TEX_VISIBLE;
			}
			else if (this->fog_of_war.is_explored(observer, tile)) {
				value = FOG_TEX_EXPLORED;
			}

			const size_t idx = (se * width + ne) * 4;
			pixels[idx] = value;
			pixels[idx + 1] = value;
			pixels[idx + 2] = value;
			pixels[idx + 3] = 255;
		}
	}

	std::unique_lock lock{this->fog_texture_mutex};
	this->fog_tile_texture.size = map_size;
	this->fog_tile_texture.pixels = std::move(pixels);
}

FogTileTexture GameState::get_fog_tile_texture() const {
	std::shared_lock lock{this->fog_texture_mutex};
	return this->fog_tile_texture;
}

namespace {

void set_minimap_pixel(std::vector<uint8_t> &pixels,
                       size_t width,
                       size_t height,
                       int ne,
                       int se,
                       uint8_t r,
                       uint8_t g,
                       uint8_t b,
                       int marker_size = 1) {
	const int half = marker_size / 2;
	for (int dy = 0; dy < marker_size; ++dy) {
		for (int dx = 0; dx < marker_size; ++dx) {
			const int x = ne + dx - half;
			const int y = se + dy - half;
			if (x < 0 || y < 0 || static_cast<size_t>(x) >= width
			    || static_cast<size_t>(y) >= height) {
				continue;
			}
			const size_t idx = (static_cast<size_t>(y) * width + static_cast<size_t>(x)) * 4;
			pixels[idx] = r;
			pixels[idx + 1] = g;
			pixels[idx + 2] = b;
			pixels[idx + 3] = 255;
		}
	}
}

} // namespace

void GameState::update_minimap_texture(const time::time_t &time) {
	if (this->map == nullptr) {
		return;
	}

	const auto &map_size = this->map->get_size();
	const size_t width = map_size[0];
	const size_t height = map_size[1];
	if (width == 0 || height == 0) {
		return;
	}

	const player_id_t observer = this->view_player_id;
	std::vector<uint8_t> pixels(width * height * 4, 0);

	for (size_t se = 0; se < height; ++se) {
		for (size_t ne = 0; ne < width; ++ne) {
			coord::tile tile{static_cast<coord::tile_t>(ne), static_cast<coord::tile_t>(se)};
			uint8_t shade = 40;
			if (this->fog_of_war.is_visible(observer, tile)) {
				shade = 120;
			}
			else if (this->fog_of_war.is_explored(observer, tile)) {
				shade = 80;
			}

			const size_t idx = (se * width + ne) * 4;
			pixels[idx] = shade;
			pixels[idx + 1] = shade;
			pixels[idx + 2] = static_cast<uint8_t>(shade - 20);
			pixels[idx + 3] = 255;
		}
	}

	for (const auto &[entity_id, entity] : this->game_entities) {
		(void) entity_id;
		if (not entity->has_component(component::component_t::POSITION)) {
			continue;
		}
		if (not entity->has_component(component::component_t::OWNERSHIP)) {
			continue;
		}

		auto ownership = std::dynamic_pointer_cast<component::Ownership>(
			entity->get_component(component::component_t::OWNERSHIP));
		auto owner_id = ownership->get_owners().get(time);

		auto pos_comp = std::dynamic_pointer_cast<component::Position>(
			entity->get_component(component::component_t::POSITION));
		auto tile = pos_comp->get_positions().get(time).to_tile();

		if (not this->fog_of_war.is_visible(observer, tile)) {
			continue;
		}

		const bool is_building = not entity->has_component(component::component_t::MOVE);
		const int marker_size = is_building ? 2 : 1;

		uint8_t r = 220;
		uint8_t g = 60;
		uint8_t b = 60;
		if (owner_id == observer) {
			r = is_building ? 120 : 60;
			g = is_building ? 180 : 140;
			b = 255;
		}

		set_minimap_pixel(pixels,
		                  width,
		                  height,
		                  static_cast<int>(tile.ne),
		                  static_cast<int>(tile.se),
		                  r,
		                  g,
		                  b,
		                  marker_size);
	}

	std::unique_lock lock{this->fog_texture_mutex};
	this->minimap_texture.size = map_size;
	this->minimap_texture.pixels = std::move(pixels);
}

MinimapTexture GameState::get_minimap_texture() const {
	std::shared_lock lock{this->fog_texture_mutex};
	return this->minimap_texture;
}

namespace {

constexpr path::cost_t HAZARD_COST_DELTA = 20;

int attack_range_tiles(const std::shared_ptr<component::Attack> &attack_component) {
	auto attack_ability = attack_component->get_ability();
	auto max_range = attack_ability.get<nyan::Float>("Attack.max_range");
	const int range = static_cast<int>(std::ceil(max_range->get()));
	return std::max(1, range);
}

} // namespace

void GameState::apply_hazard_path_costs(player_id_t for_player,
                                        path::grid_id_t grid_id,
                                        const time::time_t &time) {
	if (this->map == nullptr) {
		return;
	}

	const auto &map_size = this->map->get_size();
	const auto pathfinder = this->map->get_pathfinder();
	const auto &grid = pathfinder->get_grid(grid_id);
	const size_t sector_size = grid->get_sector_size();

	for (const auto &[entity_id, entity] : this->game_entities) {
		(void) entity_id;

		if (not entity->has_component(component::component_t::OWNERSHIP)
		    || not entity->has_component(component::component_t::POSITION)
		    || not entity->has_component(component::component_t::ATTACK)) {
			continue;
		}

		auto ownership = std::dynamic_pointer_cast<component::Ownership>(
			entity->get_component(component::component_t::OWNERSHIP));
		if (ownership->get_owners().get(time) == for_player) {
			continue;
		}

		if (entity->has_component(component::component_t::LIVE)) {
			auto live = std::dynamic_pointer_cast<component::Live>(
				entity->get_component(component::component_t::LIVE));
			if (live->get_attribute(time, HP_ATTRIBUTE) <= 0) {
				continue;
			}
		}

		auto attack = std::dynamic_pointer_cast<component::Attack>(
			entity->get_component(component::component_t::ATTACK));
		const int hazard_radius = attack_range_tiles(attack);

		auto pos_comp = std::dynamic_pointer_cast<component::Position>(
			entity->get_component(component::component_t::POSITION));
		const auto center = pos_comp->get_positions().get(time).to_tile();

		if (center.ne < 0 || center.se < 0
		    || static_cast<size_t>(center.ne) >= map_size[0]
		    || static_cast<size_t>(center.se) >= map_size[1]) {
			continue;
		}

		const auto r = static_cast<coord::tile_t>(hazard_radius);
		for (coord::tile_t dne = -r; dne <= r; ++dne) {
			for (coord::tile_t dse = -r; dse <= r; ++dse) {
				const coord::tile_t tne = center.ne + dne;
				const coord::tile_t tse = center.se + dse;
				if (tne < 0 || tse < 0
				    || static_cast<size_t>(tne) >= map_size[0]
				    || static_cast<size_t>(tse) >= map_size[1]) {
					continue;
				}

				const size_t sector_x = static_cast<size_t>(tne) / sector_size;
				const size_t sector_y = static_cast<size_t>(tse) / sector_size;
				auto sector = grid->get_sector(sector_x, sector_y);
				auto cost_field = sector->get_cost_field();

				const auto sector_origin = sector->get_position().to_tile(sector_size);
				const coord::tile_delta local{
					tne - sector_origin.ne,
					tse - sector_origin.se,
				};

				const auto field_size = static_cast<coord::tile_t>(cost_field->get_size());
				if (local.ne < 0 || local.se < 0 || local.ne >= field_size || local.se >= field_size) {
					continue;
				}

				const auto current = cost_field->get_cost(local);
				if (current == path::COST_IMPASSABLE) {
					continue;
				}

				const auto boosted = static_cast<path::cost_t>(
					std::min<int>(path::COST_MAX, current + HAZARD_COST_DELTA));
				cost_field->set_cost(local, boosted, time);
			}
		}
	}
}

void GameState::update_fog_render_visibility(const time::time_t &time) {
	const player_id_t observer = this->view_player_id;

	for (const auto &[entity_id, entity] : this->game_entities) {
		auto render_entity = entity->get_render_entity();
		if (render_entity == nullptr) {
			continue;
		}

		player_id_t owner_id = player_id_t{0};
		bool has_owner = false;
		if (entity->has_component(component::component_t::OWNERSHIP)) {
			auto ownership = std::dynamic_pointer_cast<component::Ownership>(
				entity->get_component(component::component_t::OWNERSHIP));
			owner_id = ownership->get_owners().get(time);
			has_owner = true;
		}

		if (has_owner && owner_id == observer) {
			render_entity->set_fog_display(renderer::world::fog_display_t::VISIBLE);
			continue;
		}

		if (this->is_entity_visible(observer, entity_id, time)) {
			render_entity->set_fog_display(renderer::world::fog_display_t::VISIBLE);
			continue;
		}

		auto last_known = this->get_last_known_position(observer, entity_id);
		if (last_known.has_value()) {
			render_entity->set_fog_display(renderer::world::fog_display_t::GHOST,
			                               last_known);
		}
		else {
			render_entity->set_fog_display(renderer::world::fog_display_t::HIDDEN);
		}
	}
}

void GameState::occupy_tile(coord::tile tile, entity_id_t entity) {
	// Release any tile the entity previously occupied.
	this->release_tile(entity);

	this->tile_occupants[tile] = entity;
	this->entity_tiles.insert_or_assign(entity, tile);
}

void GameState::release_tile(entity_id_t entity) {
	auto it = this->entity_tiles.find(entity);
	if (it != this->entity_tiles.end()) {
		this->tile_occupants.erase(it->second);
		this->entity_tiles.erase(it);
	}
}

bool GameState::is_tile_occupied(coord::tile tile) const {
	return this->tile_occupants.contains(tile);
}

std::optional<entity_id_t> GameState::get_tile_occupant(coord::tile tile) const {
	auto it = this->tile_occupants.find(tile);
	if (it == this->tile_occupants.end()) {
		return std::nullopt;
	}
	return it->second;
}

int64_t GameState::get_market_base_price(market_resource_t res) const {
	auto it = this->market_base_prices.find(res);
	if (it != this->market_base_prices.end()) {
		return it->second;
	}
	if (res == market_resource_t::STONE) {
		return MARKET_DEFAULT_BASE_PRICE_STONE;
	}
	return MARKET_DEFAULT_BASE_PRICE_WOOD;
}

void GameState::set_market_base_price(market_resource_t res, int64_t price) {
	this->market_base_prices[res] = std::clamp(price, MARKET_MIN_BASE_PRICE, MARKET_MAX_BASE_PRICE);
}

double GameState::get_market_fee(player_id_t player_id) const {
	auto it = this->market_fees.find(player_id);
	if (it != this->market_fees.end()) {
		return it->second;
	}
	return MARKET_DEFAULT_FEE;
}

void GameState::set_market_fee(player_id_t player_id, double fee) {
	this->market_fees[player_id] = std::max(0.0, fee);
}

int64_t GameState::get_market_buy_price(player_id_t player_id, market_resource_t res) const {
	int64_t base = this->get_market_base_price(res);
	double fee = this->get_market_fee(player_id);
	return static_cast<int64_t>(std::floor(static_cast<double>(base) * (1.0 + fee) + 0.5));
}

int64_t GameState::get_market_sell_price(player_id_t player_id, market_resource_t res) const {
	int64_t base = this->get_market_base_price(res);
	double fee = this->get_market_fee(player_id);
	return static_cast<int64_t>(std::floor(static_cast<double>(base) * (1.0 - fee) + 0.5));
}

std::string GameState::get_market_resource_fqon(market_resource_t res) const {
	auto it = this->market_resource_fqons.find(res);
	if (it != this->market_resource_fqons.end()) {
		return it->second;
	}
	switch (res) {
	case market_resource_t::FOOD:
		return "test.resource.Food";
	case market_resource_t::WOOD:
		return "test.resource.Wood";
	case market_resource_t::STONE:
		return "test.resource.Stone";
	case market_resource_t::GOLD:
	default:
		return "test.resource.Gold";
	}
}

void GameState::set_market_resource_fqon(market_resource_t res, const std::string &fqon) {
	this->market_resource_fqons[res] = fqon;
}

bool GameState::market_buy(player_id_t player_id,
                           market_resource_t res,
                           int64_t hundreds_batch,
                           const time::time_t &time) {
	if (hundreds_batch <= 0 || not this->has_player(player_id)) {
		return false;
	}

	auto &player = this->get_player(player_id);
	double fee = this->get_market_fee(player_id);
	std::string gold_fqon = this->get_market_resource_fqon(market_resource_t::GOLD);
	std::string comm_fqon = this->get_market_resource_fqon(res);

	int64_t current_base = this->get_market_base_price(res);
	int64_t total_gold_cost = 0;
	int64_t temp_base = current_base;

	for (int64_t i = 0; i < hundreds_batch; ++i) {
		int64_t step_cost = static_cast<int64_t>(std::floor(static_cast<double>(temp_base) * (1.0 + fee) + 0.5));
		total_gold_cost += step_cost;
		temp_base = std::min(MARKET_MAX_BASE_PRICE, temp_base + MARKET_PRICE_SHIFT_PER_100);
	}

	if (player->get_resource(time, gold_fqon) < total_gold_cost) {
		return false;
	}

	player->add_resource(time, gold_fqon, -total_gold_cost);
	player->add_resource(time, comm_fqon, hundreds_batch * MARKET_UNIT_BATCH);
	this->set_market_base_price(res, temp_base);
	return true;
}

bool GameState::market_sell(player_id_t player_id,
                            market_resource_t res,
                            int64_t hundreds_batch,
                            const time::time_t &time) {
	if (hundreds_batch <= 0 || not this->has_player(player_id)) {
		return false;
	}

	auto &player = this->get_player(player_id);
	double fee = this->get_market_fee(player_id);
	std::string gold_fqon = this->get_market_resource_fqon(market_resource_t::GOLD);
	std::string comm_fqon = this->get_market_resource_fqon(res);

	int64_t comm_needed = hundreds_batch * MARKET_UNIT_BATCH;
	if (player->get_resource(time, comm_fqon) < comm_needed) {
		return false;
	}

	int64_t current_base = this->get_market_base_price(res);
	int64_t total_gold_payout = 0;
	int64_t temp_base = current_base;

	for (int64_t i = 0; i < hundreds_batch; ++i) {
		int64_t step_payout = static_cast<int64_t>(std::floor(static_cast<double>(temp_base) * (1.0 - fee) + 0.5));
		total_gold_payout += step_payout;
		temp_base = std::max(MARKET_MIN_BASE_PRICE, temp_base - MARKET_PRICE_SHIFT_PER_100);
	}

	player->add_resource(time, comm_fqon, -comm_needed);
	player->add_resource(time, gold_fqon, total_gold_payout);
	this->set_market_base_price(res, temp_base);
	return true;
}

double GameState::get_tribute_fee(player_id_t player_id) const {
	auto it = this->tribute_fees.find(player_id);
	if (it != this->tribute_fees.end()) {
		return it->second;
	}
	return TRIBUTE_DEFAULT_FEE;
}

void GameState::set_tribute_fee(player_id_t player_id, double fee) {
	this->tribute_fees[player_id] = std::max(0.0, fee);
}

bool GameState::send_tribute(player_id_t sender,
                             player_id_t recipient,
                             const std::string &res_fqon,
                             int64_t amount,
                             const time::time_t &time) {
	if (amount <= 0 || sender == recipient) {
		return false;
	}
	if (not this->has_player(sender) || not this->has_player(recipient)) {
		return false;
	}

	auto &sender_player = this->get_player(sender);
	auto &recipient_player = this->get_player(recipient);

	double fee = this->get_tribute_fee(sender);
	int64_t tax = static_cast<int64_t>(std::floor(static_cast<double>(amount) * fee));
	int64_t total_cost = amount + tax;

	if (sender_player->get_resource(time, res_fqon) < total_cost) {
		return false;
	}

	sender_player->add_resource(time, res_fqon, -total_cost);
	sender_player->record_tribute_sent(time, res_fqon, amount);

	recipient_player->add_resource(time, res_fqon, amount);
	recipient_player->record_tribute_received(time, res_fqon, amount);

	return true;
}

void GameState::register_market_entity(entity_id_t id) {
	this->market_entities.insert(id);
}

void GameState::unregister_market_entity(entity_id_t id) {
	this->market_entities.erase(id);
}

bool GameState::is_market_entity(entity_id_t id) const {
	return this->market_entities.contains(id);
}

std::vector<entity_id_t> GameState::get_friendly_markets(player_id_t player_id) const {
	std::vector<entity_id_t> result;
	for (entity_id_t id : this->market_entities) {
		auto it = this->game_entities.find(id);
		if (it == this->game_entities.end()) {
			continue;
		}
		auto &entity = it->second;
		if (entity->has_component(component::component_t::OWNERSHIP)) {
			auto own = std::dynamic_pointer_cast<component::Ownership>(
				entity->get_component(component::component_t::OWNERSHIP));
			if (own && own->get_owners().get(time::TIME_MIN) == player_id) {
				result.push_back(id);
			}
		}
	}
	return result;
}

int64_t GameState::calculate_trade_gold(const coord::phys3 &p1,
                                        const coord::phys3 &p2,
                                        double map_size) const {
	double dx = std::abs(p1.ne - p2.ne);
	double dy = std::abs(p1.se - p2.se);
	double d = std::max(0.1, std::sqrt(std::pow(std::max(0.0, dx - 5.0), 2.0) +
	                                  std::pow(std::max(0.0, dy - 5.0), 2.0)));
	if (map_size <= 0.0) {
		map_size = DEFAULT_MAP_SIZE_TILES;
	}

	// Canonical AoE2 Conquerors formula:
	// gold = 2 * (d/size + 0.3) * d + 0.5
	double gold_val = 2.0 * (d / map_size + 0.3) * d + 0.5;
	return std::max<int64_t>(1, static_cast<int64_t>(std::floor(gold_val)));
}

void GameState::register_tech_definition(const TechDefinition &tech) {
	this->tech_definitions[tech.tech_id] = tech;
}

const TechDefinition* GameState::get_tech_definition(int64_t tech_id) const {
	auto it = this->tech_definitions.find(tech_id);
	if (it != this->tech_definitions.end()) {
		return &it->second;
	}
	return nullptr;
}

bool GameState::can_research(player_id_t player_id,
                             entity_id_t building_id,
                             int64_t tech_id,
                             const time::time_t &time) const {
	auto player = this->get_player(player_id);
	if (!player) {
		return false;
	}

	auto it_b = this->game_entities.find(building_id);
	if (it_b == this->game_entities.end()) {
		return false;
	}

	auto entity = it_b->second;
	if (entity->has_component(component::component_t::OWNERSHIP)) {
		auto ownership = std::dynamic_pointer_cast<component::Ownership>(
			entity->get_component(component::component_t::OWNERSHIP));
		if (ownership && ownership->get_owners().get(time) != player_id) {
			return false;
		}
	}

	if (this->is_researching(building_id)) {
		return false;
	}

	const auto *tech_def = this->get_tech_definition(tech_id);
	if (!tech_def) {
		return false;
	}

	if (player->has_researched(tech_id)) {
		return false;
	}

	if (this->is_tech_in_progress(player_id, tech_id)) {
		return false;
	}

	if (static_cast<uint8_t>(player->get_age(time)) < static_cast<uint8_t>(tech_def->required_age)) {
		return false;
	}

	for (const auto &cost_entry : tech_def->cost) {
		if (player->get_resource(time, cost_entry.resource_fqon) < cost_entry.amount) {
			return false;
		}
	}

	return true;
}

bool GameState::start_research(player_id_t player_id,
                              entity_id_t building_id,
                              int64_t tech_id,
                              const time::time_t &time) {
	if (!this->can_research(player_id, building_id, tech_id, time)) {
		return false;
	}

	auto player = this->get_player(player_id);
	const auto *tech_def = this->get_tech_definition(tech_id);

	for (const auto &cost_entry : tech_def->cost) {
		player->add_resource(time, cost_entry.resource_fqon, -cost_entry.amount);
	}

	this->active_techs_in_progress[player_id].insert(tech_id);

	time::time_t completion_time = time + tech_def->research_time_sec;

	this->building_research[building_id] = ActiveResearch{
		.building_id = building_id,
		.player_id = player_id,
		.tech_id = tech_id,
		.start_time = time,
		.completion_time = completion_time,
		.cost = tech_def->cost
	};

	return true;
}

bool GameState::cancel_research(entity_id_t building_id, const time::time_t &time) {
	auto it = this->building_research.find(building_id);
	if (it == this->building_research.end()) {
		return false;
	}

	player_id_t p_id = it->second.player_id;
	int64_t t_id = it->second.tech_id;

	if (auto player = this->get_player(p_id)) {
		for (const auto &cost_entry : it->second.cost) {
			player->add_resource(time, cost_entry.resource_fqon, cost_entry.amount);
		}
	}

	this->active_techs_in_progress[p_id].erase(t_id);
	this->building_research.erase(it);

	return true;
}

bool GameState::complete_research(entity_id_t building_id, const time::time_t &time) {
	auto it = this->building_research.find(building_id);
	if (it == this->building_research.end()) {
		return false;
	}

	ActiveResearch research = it->second;
	this->active_techs_in_progress[research.player_id].erase(research.tech_id);
	this->building_research.erase(it);

	if (auto player = this->get_player(research.player_id)) {
		player->mark_researched(research.tech_id);
	}

	const auto *tech_def = this->get_tech_definition(research.tech_id);
	if (tech_def && tech_def->effect) {
		tech_def->effect(this, research.player_id, time);
	}

	return true;
}

bool GameState::is_researching(entity_id_t building_id) const {
	return this->building_research.contains(building_id);
}

bool GameState::is_tech_in_progress(player_id_t player_id, int64_t tech_id) const {
	auto it = this->active_techs_in_progress.find(player_id);
	if (it != this->active_techs_in_progress.end()) {
		return it->second.contains(tech_id);
	}
	return false;
}

std::optional<ActiveResearch> GameState::get_active_research(entity_id_t building_id) const {
	auto it = this->building_research.find(building_id);
	if (it != this->building_research.end()) {
		return it->second;
	}
	return std::nullopt;
}

std::vector<ActiveResearch> GameState::take_completed_researches(const time::time_t &time) {
	std::vector<ActiveResearch> completed;
	std::vector<entity_id_t> to_complete;

	for (const auto &[b_id, research] : this->building_research) {
		if (research.completion_time <= time) {
			completed.push_back(research);
			to_complete.push_back(b_id);
		}
	}

	for (entity_id_t b_id : to_complete) {
		this->complete_research(b_id, time);
	}

	return completed;
}

void GameState::tick_research(const time::time_t &time) {
	this->take_completed_researches(time);
}

} // namespace openage::gamestate
