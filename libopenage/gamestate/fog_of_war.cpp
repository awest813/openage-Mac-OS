// Copyright 2026-2026 the openage authors. See copying.md for legal info.

#include "fog_of_war.h"

#include "coord/declarations.h"


namespace openage::gamestate {

const std::unordered_set<coord::tile> FogOfWar::empty_set;

void FogOfWar::update_visibility(player_id_t player, coord::tile center, int sight_range) {
	auto &visible = this->visible_tiles[player];
	auto &explored = this->explored_tiles[player];

	auto r = static_cast<coord::tile_t>(sight_range);
	for (coord::tile_t dne = -r; dne <= r; ++dne) {
		for (coord::tile_t dse = -r; dse <= r; ++dse) {
			coord::tile t{center.ne + dne, center.se + dse};
			visible.insert(t);
			explored.insert(t);
		}
	}
}

void FogOfWar::flush_visible(player_id_t player) {
	this->visible_tiles[player].clear();
}

bool FogOfWar::is_explored(player_id_t player, coord::tile tile) const {
	auto it = this->explored_tiles.find(player);
	if (it == this->explored_tiles.end()) {
		return false;
	}
	return it->second.count(tile) > 0;
}

bool FogOfWar::is_visible(player_id_t player, coord::tile tile) const {
	auto it = this->visible_tiles.find(player);
	if (it == this->visible_tiles.end()) {
		return false;
	}
	return it->second.count(tile) > 0;
}

const std::unordered_set<coord::tile> &FogOfWar::get_visible_tiles(player_id_t player) const {
	auto it = this->visible_tiles.find(player);
	if (it == this->visible_tiles.end()) {
		return FogOfWar::empty_set;
	}
	return it->second;
}

const std::unordered_set<coord::tile> &FogOfWar::get_explored_tiles(player_id_t player) const {
	auto it = this->explored_tiles.find(player);
	if (it == this->explored_tiles.end()) {
		return FogOfWar::empty_set;
	}
	return it->second;
}

void FogOfWar::set_last_known_position(player_id_t observer,
                                       entity_id_t entity,
                                       const coord::phys3 &position) {
	this->last_known_positions[observer][entity] = position;
}

std::optional<coord::phys3> FogOfWar::get_last_known_position(player_id_t observer,
                                                              entity_id_t entity) const {
	auto player_it = this->last_known_positions.find(observer);
	if (player_it == this->last_known_positions.end()) {
		return std::nullopt;
	}
	auto entity_it = player_it->second.find(entity);
	if (entity_it == player_it->second.end()) {
		return std::nullopt;
	}
	return entity_it->second;
}

void FogOfWar::clear_last_known_position(player_id_t observer, entity_id_t entity) {
	auto player_it = this->last_known_positions.find(observer);
	if (player_it != this->last_known_positions.end()) {
		player_it->second.erase(entity);
	}
}

} // namespace openage::gamestate
