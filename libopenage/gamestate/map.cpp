// Copyright 2024-2026 the openage authors. See copying.md for legal info.

#include "map.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <unordered_set>

#include <nyan/nyan.h>

#include "gamestate/api/terrain.h"
#include "gamestate/game_state.h"
#include "gamestate/terrain.h"
#include "gamestate/terrain_chunk.h"
#include "pathfinding/cost_field.h"
#include "pathfinding/definitions.h"
#include "pathfinding/grid.h"
#include "pathfinding/pathfinder.h"
#include "pathfinding/sector.h"


namespace openage::gamestate {

namespace {

bool name_contains_ci(const std::string &haystack, const std::string &needle) {
	if (needle.empty()) {
		return true;
	}
	if (haystack.size() < needle.size()) {
		return false;
	}
	auto lower = [](unsigned char c) {
		return static_cast<char>(std::tolower(c));
	};
	for (size_t i = 0; i + needle.size() <= haystack.size(); ++i) {
		bool match = true;
		for (size_t j = 0; j < needle.size(); ++j) {
			if (lower(static_cast<unsigned char>(haystack[i + j]))
			    != lower(static_cast<unsigned char>(needle[j]))) {
				match = false;
				break;
			}
		}
		if (match) {
			return true;
		}
	}
	return false;
}

} // namespace

Map::Map(const std::shared_ptr<GameState> &state,
         const std::shared_ptr<Terrain> &terrain) :
	terrain{terrain},
	pathfinder{std::make_shared<path::Pathfinder>()},
	grid_lookup{} {
	// Create a grid for each path type
	// TODO: This is non-deterministic because of the unordered set. Is this a problem?
	auto nyan_db = state->get_db_view();
	// A degenerate database — e.g. in unit tests, or a modpack that defines no
	// movement types — may not contain the PathType base object. Guard the
	// lookup so the map can still be constructed (with no pathfinding grids)
	// instead of throwing nyan::InternalError during construction.
	static const nyan::fqon_t PATH_TYPE_BASE = "engine.util.path_type.PathType";
	std::unordered_set<nyan::fqon_t> path_types;
	if (nyan_db->get_database().get_info().has_object(PATH_TYPE_BASE)) {
		path_types = nyan_db->get_obj_children_all(PATH_TYPE_BASE);
	}
	size_t grid_idx = 0;
	auto chunk_size = this->terrain->get_chunk(0)->get_size();
	auto side_length = std::max(chunk_size[0], chunk_size[1]);
	auto grid_size = this->terrain->get_chunks_size();
	for (const auto &path_type : path_types) {
		auto grid = std::make_shared<path::Grid>(grid_idx, grid_size, side_length);
		this->pathfinder->add_grid(grid);

		this->grid_lookup.emplace(path_type, grid_idx);
		grid_idx += 1;
	}

	// Set path costs
	for (size_t chunk_idx = 0; chunk_idx < this->terrain->get_chunks().size(); ++chunk_idx) {
		auto chunk_terrain = this->terrain->get_chunk(chunk_idx);
		for (size_t tile_idx = 0; tile_idx < chunk_terrain->get_tiles().size(); ++tile_idx) {
			auto tile = chunk_terrain->get_tile(tile_idx);
			auto path_costs = api::APITerrain::get_path_costs(tile.terrain);

			for (const auto &path_cost : path_costs) {
				auto grid_id = this->grid_lookup.at(path_cost.first);
				auto grid = this->pathfinder->get_grid(grid_id);

				auto sector = grid->get_sector(chunk_idx);
				auto cost_field = sector->get_cost_field();
				cost_field->set_cost(tile_idx, path_cost.second, time::TIME_ZERO);
			}
		}
	}

	// Connect sectors with portals
	for (const auto &path_type : this->grid_lookup) {
		auto grid = this->pathfinder->get_grid(path_type.second);
		grid->init_portals();
		grid->init_portal_nodes();
	}

	this->snapshot_sector_costs();
}

void Map::snapshot_sector_costs() {
	for (const auto &[path_type, grid_id] : this->grid_lookup) {
		(void) path_type;
		const auto &grid = this->pathfinder->get_grid(grid_id);
		std::vector<std::vector<path::cost_t>> grid_snaps;
		grid_snaps.reserve(grid->get_sectors().size());
		for (const auto &sector : grid->get_sectors()) {
			grid_snaps.push_back(sector->get_cost_field()->get_costs());
		}
		this->sector_cost_snapshots.emplace(grid_id, std::move(grid_snaps));
	}
}

void Map::restore_sector_costs(const path::grid_id_t grid_id, const time::time_t &time) const {
	auto snap_it = this->sector_cost_snapshots.find(grid_id);
	if (snap_it == this->sector_cost_snapshots.end()) {
		return;
	}

	const auto &grid = this->pathfinder->get_grid(grid_id);
	const auto &sectors = grid->get_sectors();
	for (size_t i = 0; i < sectors.size(); ++i) {
		sectors.at(i)->get_cost_field()->set_costs(
			std::vector<path::cost_t>(snap_it->second.at(i)),
			time);
	}
}

path_grid_kind_t Map::classify_grid(path::grid_id_t grid_id) const {
	for (const auto &[fqon, id] : this->grid_lookup) {
		if (id != grid_id) {
			continue;
		}
		if (name_contains_ci(fqon, "water")) {
			return path_grid_kind_t::WATER;
		}
		if (name_contains_ci(fqon, "land")) {
			return path_grid_kind_t::LAND;
		}
		return path_grid_kind_t::OTHER;
	}
	return path_grid_kind_t::OTHER;
}

std::optional<path::grid_id_t> Map::find_grid_by_suffix(const std::string &suffix) const {
	for (const auto &[fqon, id] : this->grid_lookup) {
		if (name_contains_ci(fqon, suffix)) {
			return id;
		}
	}
	return std::nullopt;
}

std::optional<path::cost_t> Map::get_tile_cost(path::grid_id_t grid_id, coord::tile tile) const {
	if (this->pathfinder == nullptr) {
		return std::nullopt;
	}

	const auto &map_size = this->get_size();
	if (tile.ne < 0 || tile.se < 0
	    || static_cast<size_t>(tile.ne) >= map_size[0]
	    || static_cast<size_t>(tile.se) >= map_size[1]) {
		return std::nullopt;
	}

	const auto &grid = this->pathfinder->get_grid(grid_id);
	const size_t sector_size = grid->get_sector_size();
	const size_t sector_x = static_cast<size_t>(tile.ne) / sector_size;
	const size_t sector_y = static_cast<size_t>(tile.se) / sector_size;
	auto sector = grid->get_sector(sector_x, sector_y);
	auto cost_field = sector->get_cost_field();

	const auto sector_origin = sector->get_position().to_tile(sector_size);
	const coord::tile_delta local{
		tile.ne - sector_origin.ne,
		tile.se - sector_origin.se,
	};

	const auto field_size = static_cast<coord::tile_t>(cost_field->get_size());
	if (local.ne < 0 || local.se < 0 || local.ne >= field_size || local.se >= field_size) {
		return std::nullopt;
	}

	return cost_field->get_cost(local);
}

bool Map::set_tile_cost(path::grid_id_t grid_id,
                        coord::tile tile,
                        path::cost_t cost,
                        const time::time_t &time) {
	if (this->pathfinder == nullptr) {
		return false;
	}

	const auto &map_size = this->get_size();
	if (tile.ne < 0 || tile.se < 0
	    || static_cast<size_t>(tile.ne) >= map_size[0]
	    || static_cast<size_t>(tile.se) >= map_size[1]) {
		return false;
	}

	const auto &grid = this->pathfinder->get_grid(grid_id);
	const size_t sector_size = grid->get_sector_size();
	const size_t sector_x = static_cast<size_t>(tile.ne) / sector_size;
	const size_t sector_y = static_cast<size_t>(tile.se) / sector_size;
	auto sector = grid->get_sector(sector_x, sector_y);
	auto cost_field = sector->get_cost_field();

	const auto sector_origin = sector->get_position().to_tile(sector_size);
	const coord::tile_delta local{
		tile.ne - sector_origin.ne,
		tile.se - sector_origin.se,
	};

	const auto field_size = static_cast<coord::tile_t>(cost_field->get_size());
	if (local.ne < 0 || local.se < 0 || local.ne >= field_size || local.se >= field_size) {
		return false;
	}

	cost_field->set_cost(local, cost, time);
	return true;
}

const util::Vector2s &Map::get_size() const {
	return this->terrain->get_size();
}

const std::shared_ptr<Terrain> &Map::get_terrain() const {
	return this->terrain;
}

const std::shared_ptr<path::Pathfinder> &Map::get_pathfinder() const {
	return this->pathfinder;
}

path::grid_id_t Map::get_grid_id(const nyan::fqon_t &path_grid) const {
	return this->grid_lookup.at(path_grid);
}

} // namespace openage::gamestate
