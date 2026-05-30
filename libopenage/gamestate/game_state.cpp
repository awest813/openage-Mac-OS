// Copyright 2023-2026 the openage authors. See copying.md for legal info.

#include "game_state.h"

#include <cmath>
#include <utility>

#include <nyan/nyan.h>

#include "error/error.h"
#include "event/event_loop.h"
#include "event/eventhandler.h"
#include "log/log.h"
#include "log/message.h"

#include "coord/tile.h"
#include "gamestate/component/api/attack.h"
#include "gamestate/component/api/live.h"
#include "gamestate/component/internal/position.h"
#include "gamestate/component/types.h"
#include "gamestate/component/internal/ownership.h"
#include "pathfinding/cost_field.h"
#include "pathfinding/definitions.h"
#include "pathfinding/grid.h"
#include "pathfinding/sector.h"
#include "gamestate/game_entity.h"
#include "gamestate/map.h"
#include "gamestate/definitions.h"
#include "gamestate/player.h"
#include "renderer/stages/world/render_entity.h"


namespace openage::gamestate {

GameState::GameState(const std::shared_ptr<nyan::Database> &db,
                     const std::shared_ptr<openage::event::EventLoop> &loop) :
	event::State{loop},
	event_loop{loop},
	db_view{db->new_view()},
	view_player_id{0} {
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
	this->rally_points.erase(id);
	this->release_tile(id);
}

void GameState::remove_game_entity(entity_id_t id, const time::time_t &time) {
	// Identify the owner and whether the dying entity is a building,
	// before erasing it from the index.
	player_id_t owner_id = 0;
	bool is_building = false;
	bool is_owned_unit = false;

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
	}

	this->game_entities.erase(id);
	this->carried_resources.erase(id);
	this->rally_points.erase(id);
	this->release_tile(id);

	// Release the population space a unit reserved when it was trained.
	if (is_owned_unit and this->has_player(owner_id)) {
		this->get_player(owner_id)->add_population_demand(time, -DEFAULT_POPULATION_COST);
	}

	// Remove the population headroom a destroyed building had provided.
	if (is_building and this->has_player(owner_id)) {
		this->get_player(owner_id)->add_population_capacity(time, -DEFAULT_BUILDING_POPULATION_SPACE);
	}

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

		this->update_player_visibility(owner_id, center, DEFAULT_SIGHT_RANGE_TILES);
	}

	this->update_fog_render_visibility(time);
	this->update_fog_tile_texture();
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

	// Remember the entity's position whenever it is visible to the observer.
	// Once it leaves vision we keep that remembered spot untouched so it can
	// be rendered as a "ghost" at the place it was last seen. Entities the
	// observer has never seen have no recorded position and stay hidden.
	// Because is_entity_visible is a const query we use mutable fog_of_war.
	if (visible) {
		const_cast<FogOfWar &>(this->fog_of_war).set_last_known_position(observer, entity_id, pos);
	}

	return visible;
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

} // namespace openage::gamestate
