// Copyright 2023-2026 the openage authors. See copying.md for legal info.

#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "coord/tile.h"
#include "event/state.h"
#include "gamestate/fog_of_war.h"
#include "gamestate/player.h"
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
 * Resources currently carried by a gatherer entity that have not yet been
 * dropped off at a drop-off building.
 */
struct CarriedResource {
	/**
	 * Resource type identifier (nyan fqon).
	 */
	std::string resource_type;

	/**
	 * Amount carried.
	 */
	int64_t amount;
};

/**
 * State of the game.
 *
 * Contains index structures for looking up game entities and other
 * information for the current game.
 *
 * NOTE: GameState must always be constructed via std::make_shared. It derives
 * from std::enable_shared_from_this and uses shared_from_this() (e.g. when
 * firing game-over events); a non-shared construction would throw
 * std::bad_weak_ptr.
 */
class GameState : public openage::event::State,
                  public std::enable_shared_from_this<GameState> {
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
	 * Called when an entity is destroyed (e.g. HP reaches 0) without
	 * defeat checking (e.g. resource depletion).
	 *
	 * @param id ID of the game entity to remove.
	 */
	void remove_game_entity(entity_id_t id);

	/**
	 * Remove a game entity from the index and check whether its owner
	 * has been defeated (lost all buildings).
	 *
	 * Use this overload when destroying combat or production entities.
	 *
	 * @param id   ID of the game entity to remove.
	 * @param time Simulation time of the destruction.
	 */
	void remove_game_entity(entity_id_t id, const time::time_t &time);

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
	 * Check whether a player with the given ID exists.
	 *
	 * @param id ID of the player.
	 *
	 * @return true if the player exists, false otherwise.
	 */
	bool has_player(player_id_t id) const;

	/**
	 * Get all players in the current game.
	 *
	 * @return Map of all players by their ID.
	 */
	const std::unordered_map<player_id_t, std::shared_ptr<Player>> &get_players() const;

	/**
	 * Count how many players are still in the ALIVE state.
	 *
	 * @return Number of alive players.
	 */
	size_t get_alive_player_count() const;

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
	 * Check whether a gatherer entity is currently carrying resources that have
	 * not yet been dropped off.
	 *
	 * @param id Gatherer entity ID.
	 *
	 * @return true if the entity is carrying resources, false otherwise.
	 */
	bool is_carrying_resources(entity_id_t id) const;

	/**
	 * Get the resources carried by a gatherer entity.
	 *
	 * @param id Gatherer entity ID.
	 *
	 * @return Carried resource, or std::nullopt if the entity carries nothing.
	 */
	std::optional<CarriedResource> get_carried_resource(entity_id_t id) const;

	/**
	 * Record the resources carried by a gatherer entity.
	 *
	 * @param id            Gatherer entity ID.
	 * @param resource_type Resource type identifier (nyan fqon).
	 * @param amount        Amount carried.
	 */
	void set_carried_resource(entity_id_t id,
	                          const std::string &resource_type,
	                          int64_t amount);

	/**
	 * Clear any resources carried by a gatherer entity (e.g. after drop-off
	 * or when the entity is destroyed).
	 *
	 * @param id Gatherer entity ID.
	 */
	void clear_carried_resource(entity_id_t id);

	/**
	 * TODO: Only for testing.
	 */
	const std::shared_ptr<assets::ModManager> &get_mod_manager() const;
	void set_mod_manager(const std::shared_ptr<assets::ModManager> &mod_manager);

	// -----------------------------------------------------------------------
	// Fog of War
	// -----------------------------------------------------------------------

	/**
	 * Update the visible tile set for a player around a single unit position.
	 *
	 * Should be called once per living unit each tick, after
	 * flush_player_visibility() has been called to reset the set.
	 *
	 * @param player      Player whose visibility is updated.
	 * @param center      Tile at the centre of the unit's sight cone.
	 * @param sight_range Chebyshev radius in tiles.
	 */
	void update_player_visibility(player_id_t player,
	                              coord::tile center,
	                              int sight_range);

	/**
	 * Clear the currently-visible tile set for a player so it can be rebuilt
	 * for the next tick.
	 *
	 * @param player Player whose visible set is cleared.
	 */
	void flush_player_visibility(player_id_t player);

	/**
	 * Check whether a tile is currently visible to a player.
	 *
	 * @param player Player performing the query.
	 * @param tile   Tile to test.
	 * @return true if the tile is in the player's current line of sight.
	 */
	bool is_tile_visible(player_id_t player, coord::tile tile) const;

	/**
	 * Check whether a tile has ever been explored by a player.
	 *
	 * @param player Player performing the query.
	 * @param tile   Tile to test.
	 * @return true if the tile has been seen at least once.
	 */
	bool is_tile_explored(player_id_t player, coord::tile tile) const;

	/**
	 * Check whether a game entity is currently visible to the observer player.
	 *
	 * The entity's position at \p time is converted to a tile and compared
	 * against the observer's current visible tile set.
	 *
	 * @param observer  Player performing the query.
	 * @param entity_id ID of the entity being checked.
	 * @param time      Simulation time at which to sample the entity's position.
	 * @return true if the entity's tile is visible to the observer.
	 */
	bool is_entity_visible(player_id_t observer,
	                       entity_id_t entity_id,
	                       const time::time_t &time) const;

	/**
	 * Get the last-known position of an entity as seen by a player.
	 *
	 * When an entity leaves a player's vision, its most recent position is
	 * stored. This can be used to show "ghost" units on the map.
	 *
	 * @param observer  Player who saw the entity.
	 * @param entity_id ID of the entity.
	 * @return The last-known position, or std::nullopt if the entity has
	 *         never been seen by the observer.
	 */
	std::optional<coord::phys3> get_last_known_position(player_id_t observer,
	                                                    entity_id_t entity_id) const;

	// -----------------------------------------------------------------------
	// Tile Occupancy (collision avoidance)
	// -----------------------------------------------------------------------

	/**
	 * Mark a tile as occupied by the given entity.
	 *
	 * @param tile   Tile to occupy.
	 * @param entity Entity occupying the tile.
	 */
	void occupy_tile(coord::tile tile, entity_id_t entity);

	/**
	 * Release any tile previously occupied by the given entity.
	 *
	 * @param entity Entity releasing its tile.
	 */
	void release_tile(entity_id_t entity);

	/**
	 * Check whether a tile is currently occupied by any entity.
	 *
	 * @param tile Tile to test.
	 * @return true if some entity occupies the tile.
	 */
	bool is_tile_occupied(coord::tile tile) const;

	/**
	 * Get the entity occupying a tile, if any.
	 *
	 * @param tile Tile to query.
	 * @return The occupying entity's ID, or std::nullopt.
	 */
	std::optional<entity_id_t> get_tile_occupant(coord::tile tile) const;

private:
	/**
	 * Check whether a player has been defeated (lost all buildings) after the
	 * entity with the given ID was removed.  If defeated, mark the player and
	 * check whether a sole survivor has won the game.  Fires
	 * "game.player_defeated" and "game.game_over" events as appropriate.
	 *
	 * @param owner_id ID of the player whose entity was just removed.
	 * @param time     Simulation time of the destruction.
	 */
	void check_defeat(player_id_t owner_id, const time::time_t &time);

	/**
	 * Event loop — used to fire player-defeated and game-over events.
	 */
	std::shared_ptr<openage::event::EventLoop> event_loop;

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
	 * Resources carried by gatherer entities, keyed by entity ID.
	 *
	 * Part of the game state (rather than a global) so it is deterministic per
	 * simulation and is cleaned up when an entity is destroyed.
	 */
	std::unordered_map<entity_id_t, CarriedResource> carried_resources;

	/**
	 * Fog-of-war state for all players.
	 */
	FogOfWar fog_of_war;

	/**
	 * Tile occupancy: maps a tile to the entity currently occupying it.
	 *
	 * Used for collision avoidance: the Move system checks this before
	 * placing a unit on a destination tile.
	 */
	std::unordered_map<coord::tile, entity_id_t> tile_occupants;

	/**
	 * Reverse map: entity → the tile it currently occupies.
	 */
	std::unordered_map<entity_id_t, coord::tile> entity_tiles;

	/**
	 * TODO: Only for testing
	 */
	std::shared_ptr<assets::ModManager> mod_manager;
};
} // namespace gamestate
} // namespace openage
