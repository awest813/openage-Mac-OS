// Copyright 2023-2024 the openage authors. See copying.md for legal info.

#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "event/state.h"
#include "gamestate/types.h"
#include "time/time.h"


namespace nyan {
class Database;
class View;
} // namespace nyan

namespace openage {

namespace assets {
class ModManager;
}

namespace event {
class EventLoop;
}

namespace gamestate {
class GameEntity;
class Map;
class Player;

/**
 * A pending unit production request created by the Production system.
 *
 * Systems cannot reach the EntityFactory directly, so a completed training
 * action is recorded here. The simulation drains finished requests (those
 * whose completion time has passed) and spawns the corresponding entity.
 */
struct ProductionRequest {
	/**
	 * Player that owns (and paid for) the produced entity.
	 */
	player_id_t owner;

	/**
	 * nyan fqon of the game entity to spawn.
	 */
	std::string game_entity;

	/**
	 * Simulation time at which the entity finishes training.
	 */
	time::time_t completion_time;
};

/**
 * State of the game.
 *
 * Contains index structures for looking up game entities and other
 * information for the current game.
 */
class GameState : public openage::event::State {
public:
	/**
	 * Create a new game state.
	 *
	 * @param db Nyan game data database.
	 * @param event_loop Event loop for the game state.
	 */
	explicit GameState(const std::shared_ptr<nyan::Database> &db,
	                   const std::shared_ptr<openage::event::EventLoop> &event_loop);

	/**
	 * Get the nyan database view for the whole game.
	 *
	 * Players have individual views for their own data.
	 *
	 * @return nyan database view.
	 */
	const std::shared_ptr<nyan::View> &get_db_view();

	/**
	 * Add a new game entity to the index.
	 *
	 * @param entity New game entity.
	 */
	void add_game_entity(const std::shared_ptr<GameEntity> &entity);

	/**
	 * Remove a game entity from the index.
	 *
	 * Called when an entity is destroyed (e.g. HP reaches 0).
	 *
	 * @param id ID of the game entity to remove.
	 */
	void remove_game_entity(entity_id_t id);

	/**
	 * Add a new player to the index.
	 *
	 * @param player New player.
	 */
	void add_player(const std::shared_ptr<Player> &player);

	/**
	 * Set the map of the current game.
	 *
	 * @param terrain Map object.
	 */
	void set_map(const std::shared_ptr<Map> &map);

	/**
	 * Get a game entity by its ID.
	 *
	 * @param id ID of the game entity.
	 *
	 * @return Game entity with the given ID.
	 */
	const std::shared_ptr<GameEntity> &get_game_entity(entity_id_t id) const;

	/**
	 * Get all game entities in the current game.
	 *
	 * @return Map of all game entities in the current game by their ID.
	 */
	const std::unordered_map<entity_id_t, std::shared_ptr<GameEntity>> &get_game_entities() const;

	/**
	 * Get a player by its ID.
	 *
	 * @param id ID of the player.
	 *
	 * @return Player with the given ID.
	 */
	const std::shared_ptr<Player> &get_player(player_id_t id) const;

	/**
	 * Get the map of the current game.
	 *
	 * @return Map object.
	 */
	const std::shared_ptr<Map> &get_map() const;

	/**
	 * Queue a unit production request.
	 *
	 * Called by the Production system once a unit has been paid for and its
	 * training timer started. The request is fulfilled (the entity spawned)
	 * by the simulation once its completion time has passed.
	 *
	 * @param owner           Player that owns the produced entity.
	 * @param game_entity     nyan fqon of the game entity to spawn.
	 * @param completion_time Simulation time at which training finishes.
	 */
	void request_production(player_id_t owner,
	                        const std::string &game_entity,
	                        const time::time_t &completion_time);

	/**
	 * Remove and return all production requests that have finished by the
	 * given time.
	 *
	 * @param time Current simulation time.
	 *
	 * @return Completed production requests (in insertion order).
	 */
	std::vector<ProductionRequest> take_completed_productions(const time::time_t &time);

	/**
	 * Get the number of production requests still pending.
	 *
	 * @return Number of queued (unfinished and finished-but-undrained) requests.
	 */
	size_t pending_production_count() const;

	/**
	 * TODO: Only for testing.
	 */
	const std::shared_ptr<assets::ModManager> &get_mod_manager() const;
	void set_mod_manager(const std::shared_ptr<assets::ModManager> &mod_manager);

private:
	/**
	 * View for the nyan game data database.
	 */
	std::shared_ptr<nyan::View> db_view;

	/**
	 * Map of all game entities in the current game by their ID.
	 */
	std::unordered_map<entity_id_t, std::shared_ptr<GameEntity>> game_entities;

	/**
	 * Map of all players in the current game by their ID.
	 */
	std::unordered_map<player_id_t, std::shared_ptr<Player>> players;

	/**
	 * Map of the current game.
	 */
	std::shared_ptr<Map> map;

	/**
	 * Pending unit production requests, ordered by insertion.
	 */
	std::vector<ProductionRequest> production_requests;

	/**
	 * TODO: Only for testing
	 */
	std::shared_ptr<assets::ModManager> mod_manager;
};
} // namespace gamestate
} // namespace openage
