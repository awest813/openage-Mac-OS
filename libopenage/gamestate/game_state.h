// Copyright 2023-2026 the openage authors. See copying.md for legal info.

#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "coord/tile.h"
#include "event/state.h"
#include "gamestate/definitions.h"
#include "pathfinding/types.h"
#include "gamestate/fog_of_war.h"
#include "gamestate/player.h"
#include "gamestate/types.h"
#include "time/time.h"
#include "util/vector.h"


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
 * CPU-side fog map for terrain rendering (one RGBA texel per map tile).
 *
 * The red channel encodes visibility: 0 = unexplored, 128 = explored but not
 * currently visible, 255 = in line of sight.
 */
struct FogTileTexture {
	util::Vector2s size{0, 0};
	std::vector<uint8_t> pixels;

	bool empty() const {
		return this->pixels.empty();
	}
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
 * One resource line in a building's recorded construction cost.
 */
struct ResourceCostEntry {
	std::string resource_type;
	int64_t amount = 0;
};

/**
 * Construction cost recorded for a completed building (used for salvage).
 */
struct BuildingCostRecord {
	std::vector<ResourceCostEntry> entries;
	/** Salvage fraction when the building is destroyed (not deconstructed). */
	double destroy_recovery_fraction = SALVAGE_RECOVERY_FRACTION;
	/** Salvage fraction when the player deconstructs the building. */
	double deconstruct_recovery_fraction = DECONSTRUCT_RECOVERY_FRACTION;
	/** Villager deconstruct duration in seconds. */
	double deconstruct_time = 0.0;

	bool empty() const {
		return this->entries.empty();
	}
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
	 * Check whether a producing entity (e.g. a building) has a rally point set.
	 *
	 * @param id Producer entity ID.
	 *
	 * @return true if a rally point is set, false otherwise.
	 */
	bool has_rally_point(entity_id_t id) const;

	/**
	 * Get the rally point of a producing entity. Newly produced units are sent
	 * here automatically.
	 *
	 * @param id Producer entity ID.
	 *
	 * @return Rally point position, or std::nullopt if none is set.
	 */
	std::optional<coord::phys3> get_rally_point(entity_id_t id) const;

	/**
	 * Set the rally point of a producing entity.
	 *
	 * @param id     Producer entity ID.
	 * @param target Rally point position.
	 */
	void set_rally_point(entity_id_t id, const coord::phys3 &target);

	/**
	 * Clear a producing entity's rally point (e.g. when it is destroyed).
	 *
	 * @param id Producer entity ID.
	 */
	void clear_rally_point(entity_id_t id);

	/**
	 * Record the construction cost of a completed building (for salvage on destroy).
	 */
	void set_building_cost(entity_id_t id, BuildingCostRecord cost);

	/**
	 * @return Recorded construction cost, or std::nullopt if unknown.
	 */
	std::optional<BuildingCostRecord> get_building_cost(entity_id_t id) const;

	void clear_building_cost(entity_id_t id);

	/**
	 * Record population demand reserved for a living or in-production unit.
	 */
	void set_entity_population_demand(entity_id_t id, int64_t amount);

	/**
	 * @return Recorded population demand, or std::nullopt if unknown.
	 */
	std::optional<int64_t> get_entity_population_demand(entity_id_t id) const;

	/**
	 * Record population headroom provided by a completed building.
	 */
	void set_entity_population_provision(entity_id_t id, int64_t amount);

	/**
	 * @return Recorded population provision, or std::nullopt if unknown.
	 */
	std::optional<int64_t> get_entity_population_provision(entity_id_t id) const;

	/**
	 * Decay salvage piles and remove depleted ones. Call once per simulation tick.
	 */
	void tick_salvage_decay(const time::time_t &time);

	/**
	 * Complete a scheduled deconstruction: spawn salvage at \p position and remove
	 * the building if it still exists (e.g. not destroyed by combat in the meantime).
	 */
	void complete_deconstruction(entity_id_t building_id,
	                             const coord::phys3 &position,
	                             const BuildingCostRecord &cost,
	                             double recovery_fraction,
	                             const time::time_t &time);

	/**
	 * TODO: Only for testing.
	 */
	const std::shared_ptr<assets::ModManager> &get_mod_manager() const;
	void set_mod_manager(const std::shared_ptr<assets::ModManager> &mod_manager);

	// -----------------------------------------------------------------------
	// Fog of War
	// -----------------------------------------------------------------------

	/**
	 * Rebuild fog-of-war visibility for all players at the given time.
	 *
	 * Flushes each player's visible tile set, then reveals tiles around every
	 * entity that currently provides line of sight (position + ownership, and
	 * HP > 0 when a Live component is present).
	 *
	 * @param time Simulation time at which entity positions are sampled.
	 */
	void refresh_visibility(const time::time_t &time);

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

	/**
	 * Set the player whose fog-of-war view drives rendering.
	 *
	 * Defaults to player 0. Used by \p update_fog_render_visibility().
	 *
	 * @param player Local observer player ID.
	 */
	void set_view_player(player_id_t player);

	/**
	 * @return Player ID used for fog-of-war rendering queries.
	 */
	player_id_t get_view_player() const;

	/**
	 * Update each entity's render visibility from the view player's fog state.
	 *
	 * Own units are always shown. Enemy units outside line of sight are hidden;
	 * units that left vision are drawn as ghosts at their last-known position.
	 *
	 * Call after \p refresh_visibility() each simulation tick.
	 *
	 * @param time Simulation time at which entity positions are sampled.
	 */
	void update_fog_render_visibility(const time::time_t &time);

	/**
	 * Get a snapshot of the fog tile texture for terrain rendering.
	 *
	 * Thread-safe. Returns an empty texture if no map is loaded yet.
	 *
	 * @return Fog texture data for the view player.
	 */
	FogTileTexture get_fog_tile_texture() const;

	/**
	 * Raise movement costs on tiles within range of enemy attackers.
	 *
	 * Used before pathfinding so units path around hostile towers and castles.
	 * Call \p Map::restore_sector_costs() before and after applying hazards.
	 *
	 * @param for_player  Player whose units are pathfinding (enemies pose hazards).
	 * @param grid_id     Pathfinding grid to modify.
	 * @param time        Time stamp for cost-field invalidation.
	 */
	void apply_hazard_path_costs(player_id_t for_player,
	                             path::grid_id_t grid_id,
	                             const time::time_t &time);

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

	void spawn_salvage_pile(const coord::phys3 &position,
	                        const BuildingCostRecord &cost,
	                        double recovery_fraction,
	                        const time::time_t &time);

	void finish_deconstruct(entity_id_t building_id, const time::time_t &time);

	entity_id_t allocate_entity_id() const;

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
	 * Rally points of producing entities, keyed by entity ID. Units produced by
	 * an entity with a rally point are automatically sent there on spawn.
	 *
	 * Part of the game state (rather than a global) so it is deterministic per
	 * simulation and is cleaned up when an entity is destroyed.
	 */
	std::unordered_map<entity_id_t, coord::phys3> rally_points;

	/**
	 * Construction costs of completed buildings, keyed by entity ID.
	 */
	std::unordered_map<entity_id_t, BuildingCostRecord> building_costs;

	/**
	 * Population demand reserved by units, keyed by entity ID.
	 */
	std::unordered_map<entity_id_t, int64_t> entity_population_demand;

	/**
	 * Population headroom provided by buildings, keyed by entity ID.
	 */
	std::unordered_map<entity_id_t, int64_t> entity_population_provision;

	/**
	 * Entity IDs of active salvage piles (for periodic decay).
	 */
	std::unordered_set<entity_id_t> salvage_pile_ids;

	/**
	 * Fog-of-war state for all players.
	 */
	FogOfWar fog_of_war;

	/**
	 * Player whose fog-of-war view is used for rendering (local player).
	 */
	player_id_t view_player_id;

	/**
	 * Per-tile fog values for terrain shading, rebuilt each visibility tick.
	 */
	FogTileTexture fog_tile_texture;

	/**
	 * Protects \p fog_tile_texture between simulation and presenter threads.
	 */
	mutable std::shared_mutex fog_texture_mutex;

	/**
	 * Rebuild \p fog_tile_texture from the current fog-of-war state.
	 */
	void update_fog_tile_texture();

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
