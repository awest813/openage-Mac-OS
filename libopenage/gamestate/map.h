// Copyright 2024-2026 the openage authors. See copying.md for legal info.

#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

#include <nyan/nyan.h>

#include "coord/tile.h"
#include "pathfinding/types.h"
#include "time/time.h"
#include "util/vector.h"


namespace openage {
namespace path {
class Pathfinder;
} // namespace path

namespace gamestate {
class GameState;
class Terrain;

/**
 * Classification of a pathfinding grid by its nyan PathType name.
 */
enum class path_grid_kind_t : uint8_t {
	OTHER = 0,
	LAND = 1,
	WATER = 2,
};

class Map {
public:
	/**
	 * Create a new map from existing terrain.
	 *
	 * Initializes the pathfinder with the terrain path costs.
	 *
	 * @param state Game state.
	 * @param terrain Terrain object.
	 */
	Map(const std::shared_ptr<GameState> &state,
	    const std::shared_ptr<Terrain> &terrain);

	~Map() = default;

	/**
	 * Get the size of the map.
	 *
	 * @return Map size (width x height).
	 */
	const util::Vector2s &get_size() const;

	/**
	 * Get the terrain of the map.
	 *
	 * @return Terrain.
	 */
	const std::shared_ptr<Terrain> &get_terrain() const;

	/**
	 * Get the pathfinder for the map.
	 *
	 * @return Pathfinder.
	 */
	const std::shared_ptr<path::Pathfinder> &get_pathfinder() const;

	/**
	 * Get the grid ID associated with a nyan path grid object.
	 *
	 * @param path_grid Path grid object fqon.
	 *
	 * @return Grid ID.
	 */
	path::grid_id_t get_grid_id(const nyan::fqon_t &path_grid) const;

	/**
	 * Restore pathfinding costs on a grid to the values captured at map creation.
	 *
	 * @param grid_id Grid to restore.
	 * @param time    Time stamp used to invalidate cached flow fields.
	 */
	void restore_sector_costs(path::grid_id_t grid_id, const time::time_t &time) const;

	/**
	 * Classify a pathfinding grid from its registered nyan PathType fqon.
	 *
	 * @param grid_id Grid to classify.
	 * @return LAND / WATER when the fqon contains those tokens, else OTHER.
	 */
	path_grid_kind_t classify_grid(path::grid_id_t grid_id) const;

	/**
	 * Find the first registered grid whose PathType fqon contains \p suffix
	 * (case-insensitive).
	 *
	 * @param suffix Substring to match (e.g. "Land", "Water").
	 * @return Grid ID, or std::nullopt if none match.
	 */
	std::optional<path::grid_id_t> find_grid_by_suffix(const std::string &suffix) const;

	/**
	 * Read the live pathfinding cost of a world tile on a grid.
	 *
	 * @return Cost, or std::nullopt if the tile/grid is out of range.
	 */
	std::optional<path::cost_t> get_tile_cost(path::grid_id_t grid_id, coord::tile tile) const;

	/**
	 * Write the live pathfinding cost of a world tile on a grid.
	 *
	 * Does not update the baseline snapshot (transient overlays like hazards
	 * and bridges re-apply after restore_sector_costs).
	 *
	 * @return true if the cost was written.
	 */
	bool set_tile_cost(path::grid_id_t grid_id,
	                   coord::tile tile,
	                   path::cost_t cost,
	                   const time::time_t &time);

private:
	/**
	 * Capture baseline pathfinding costs after terrain setup.
	 */
	void snapshot_sector_costs();

	/**
	 * Baseline sector cost fields per path grid, indexed by sector ID.
	 */
	std::unordered_map<path::grid_id_t, std::vector<std::vector<path::cost_t>>> sector_cost_snapshots;

	/**
	 * Terrain.
	 */
	std::shared_ptr<Terrain> terrain;

	/**
	 * Pathfinder.
	 */
	std::shared_ptr<path::Pathfinder> pathfinder;

	/**
	 * Lookup table for mapping path grid objects in nyan to grid indices.
	 */
	std::unordered_map<nyan::fqon_t, path::grid_id_t> grid_lookup;
};

} // namespace gamestate
} // namespace openage
