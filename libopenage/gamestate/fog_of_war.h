// Copyright 2026-2026 the openage authors. See copying.md for legal info.

#pragma once

#include <optional>
#include <unordered_map>
#include <unordered_set>

#include "coord/phys.h"
#include "coord/tile.h"
#include "gamestate/types.h"


namespace openage::gamestate {

/**
 * Manages fog-of-war state for all players.
 *
 * Two maps are maintained per player:
 *  - explored_tiles: tiles ever seen (permanently revealed on the minimap).
 *  - visible_tiles:  tiles currently in line-of-sight of at least one unit.
 *
 * Visibility is refreshed each simulation tick by calling update_visibility()
 * for every living unit; visible_tiles is fully rebuilt each update cycle.
 */
class FogOfWar {
public:
	FogOfWar() = default;

	/**
	 * Rebuild the visible tile set for a player around a single unit.
	 *
	 * The tile \p center and every tile within \p sight_range (Chebyshev
	 * distance) are added to both visible_tiles and explored_tiles.
	 *
	 * This method is *additive*: call it once per living unit, then call
	 * flush_visible() at the start of each new game tick to wipe the old
	 * visible set before re-populating it.
	 *
	 * @param player   Player whose visibility is being updated.
	 * @param center   Tile position of the unit.
	 * @param sight_range  Chebyshev radius in tiles.
	 */
	void update_visibility(player_id_t player, coord::tile center, int sight_range);

	/**
	 * Clear the visible tile set for a player in preparation for a new tick.
	 *
	 * @param player  Player whose visible set is cleared.
	 */
	void flush_visible(player_id_t player);

	/**
	 * @returns true if \p tile has ever been explored by \p player.
	 */
	bool is_explored(player_id_t player, coord::tile tile) const;

	/**
	 * @returns true if \p tile is currently visible to \p player.
	 */
	bool is_visible(player_id_t player, coord::tile tile) const;

	/**
	 * @returns the set of tiles currently visible to \p player.
	 *          Returns an empty set if the player has no entries.
	 */
	const std::unordered_set<coord::tile> &get_visible_tiles(player_id_t player) const;

	/**
	 * @returns the set of tiles ever explored by \p player.
	 *          Returns an empty set if the player has no entries.
	 */
	const std::unordered_set<coord::tile> &get_explored_tiles(player_id_t player) const;

	// -----------------------------------------------------------------------
	// Last-known position tracking
	// -----------------------------------------------------------------------

	/**
	 * Record the last-known position of an entity for a specific observer.
	 *
	 * Called when an entity leaves the observer's line of sight.
	 *
	 * @param observer  Observing player.
	 * @param entity    Entity whose position is recorded.
	 * @param position  The entity's position at the moment it left vision.
	 */
	void set_last_known_position(player_id_t observer,
	                             entity_id_t entity,
	                             const coord::phys3 &position);

	/**
	 * Get the last-known position of an entity for a specific observer.
	 *
	 * @param observer  Observing player.
	 * @param entity    Entity to query.
	 * @return The last-known position, or std::nullopt if never recorded.
	 */
	std::optional<coord::phys3> get_last_known_position(player_id_t observer,
	                                                    entity_id_t entity) const;

	/**
	 * Clear the last-known position entry for an entity (e.g. when it becomes
	 * visible again).
	 *
	 * @param observer  Observing player.
	 * @param entity    Entity to clear.
	 */
	void clear_last_known_position(player_id_t observer, entity_id_t entity);

private:
	/// Tiles currently visible to each player (rebuilt each tick).
	std::unordered_map<player_id_t, std::unordered_set<coord::tile>> visible_tiles;

	/// Tiles ever seen by each player (never cleared).
	std::unordered_map<player_id_t, std::unordered_set<coord::tile>> explored_tiles;

	/// Empty set returned by reference for players with no entries.
	static const std::unordered_set<coord::tile> empty_set;

	/**
	 * Last-known position of entities per observer player.
	 * Outer key: observer player ID.
	 * Inner key: entity ID.
	 * Value:     world position when the entity was last visible.
	 */
	std::unordered_map<player_id_t,
	                   std::unordered_map<entity_id_t, coord::phys3>> last_known_positions;
};

} // namespace openage::gamestate
