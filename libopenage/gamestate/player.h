// Copyright 2018-2024 the openage authors. See copying.md for legal info.

#pragma once

#include <cstdint>
#include <memory>
#include <unordered_map>

#include <nyan/nyan.h>

#include "curve/discrete.h"
#include "gamestate/types.h"
#include "time/time.h"


namespace nyan {
class View;
} // namespace nyan

namespace openage {
namespace event {
class EventLoop;
}

namespace gamestate {

/**
 * Entity for managing a player inside the game world.
 */
class Player {
public:
	/**
	 * Create a new player with the given id.
	 *
	 * @param id Unique player ID.
	 * @param db_view View of the nyan database used for the player.
	 */
	Player(player_id_t id,
	       const std::shared_ptr<nyan::View> &db_view);

	Player(Player &&) = default;
	Player &operator=(Player &&) = default;

	~Player() = default;

	/**
	 * Copy this player.
	 *
	 * @param id Unique identifier.
	 *
	 * @return Copy of this player.
	 */
	std::shared_ptr<Player> copy(entity_id_t id);

	/**
	 * Get the unique ID of the player.
	 *
	 * @return Unique player ID.
	 */
	player_id_t get_id() const;

	/**
	 * Get the nyan game database view for the player.
	 *
	 * @return nyan database view.
	 */
	const std::shared_ptr<nyan::View> &get_db_view() const;

	/**
	 * Initialize a resource type for this player (called during setup).
	 *
	 * @param time       Time at which the resource is initialised.
	 * @param loop       Event loop used for the curve.
	 * @param resource   Resource type identifier (nyan fqon).
	 * @param amount     Starting amount.
	 */
	void init_resource(const time::time_t &time,
	                   const std::shared_ptr<openage::event::EventLoop> &loop,
	                   const nyan::fqon_t &resource,
	                   int64_t amount = 0);

	/**
	 * Get the current amount of a resource.
	 *
	 * @param time     Time at which to read.
	 * @param resource Resource type identifier (nyan fqon).
	 *
	 * @return Current resource amount, or 0 if the resource is unknown.
	 */
	int64_t get_resource(const time::time_t &time, const nyan::fqon_t &resource) const;

	/**
	 * Add an amount to a resource.
	 *
	 * If the resource type is not yet tracked it is silently ignored.
	 *
	 * @param time     Simulation time of the change.
	 * @param resource Resource type identifier (nyan fqon).
	 * @param amount   Amount to add (may be negative to subtract).
	 */
	void add_resource(const time::time_t &time,
	                  const nyan::fqon_t &resource,
	                  int64_t amount);

protected:
	/**
	 * A player cannot be default copied because of their unique ID.
	 *
	 * \p copy() must be used instead.
	 */
	Player(const Player &) = default;
	Player &operator=(const Player &) = default;

private:
	/**
	 * Set the unique identifier of this player.
	 *
	 * Only called by \p copy().
	 *
	 * @param id New ID.
	 */
	void set_id(entity_id_t id);

	/**
	 * Player ID. Must be unique.
	 */
	player_id_t id;

	/**
	 * Player view of the nyan game data database.
	 */
	std::shared_ptr<nyan::View> db_view;

	/**
	 * Resource amounts per resource type (keyed by nyan fqon).
	 * Each entry is a time-indexed discrete curve so that resource changes
	 * are recorded deterministically.
	 */
	std::unordered_map<nyan::fqon_t, std::shared_ptr<curve::Discrete<int64_t>>> resources;
};

} // namespace gamestate
} // namespace openage
