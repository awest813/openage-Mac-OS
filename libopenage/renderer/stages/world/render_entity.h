// Copyright 2022-2024 the openage authors. See copying.md for legal info.

#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "coord/phys.h"
#include "coord/scene.h"
#include "curve/continuous.h"
#include "curve/discrete.h"
#include "curve/segmented.h"
#include "renderer/stages/render_entity.h"


namespace openage::renderer::world {

/**
 * How an entity is drawn under fog of war.
 */
enum class fog_display_t {
	/** Entity is in the observer's line of sight (or owned by them). */
	VISIBLE,
	/** Entity is outside vision and has no last-known position to show. */
	HIDDEN,
	/** Entity left vision; draw a ghost at the last-known position. */
	GHOST,
};

/**
 * Render entity for pushing updates to the World renderer.
 */
class RenderEntity final : public renderer::RenderEntity {
public:
	RenderEntity();
	~RenderEntity() = default;

	/**
	 * Update the render entity with information from the gamestate.
	 *
	 * Updating the render entity with this method is thread-safe.
	 *
	 * @param ref_id Game entity ID.
	 * @param position Position of the game entity inside the game world.
	 * @param angle Angle of the game entity inside the game world.
	 * @param animation_path Path to the animation definition.
	 * @param time Simulation time of the update.
	 */
	void update(const uint32_t ref_id,
	            const curve::Continuous<coord::phys3> &position,
	            const curve::Segmented<coord::phys_angle_t> &angle,
	            const std::string animation_path,
	            const time::time_t time = 0.0);

	/**
	 * This function is for DEBUGGING and should not be used.
	 *
	 * Update the render entity with information from the gamestate.
	 *
	 * Updating the render entity with this method is thread-safe.
	 *
	 * @param ref_id Game entity ID.
	 * @param position Position of the game entity inside the game world.
	 * @param animation_path Path to the animation definition.
	 * @param time Simulation time of the update.
	 */
	void update(const uint32_t ref_id,
	            const coord::phys3 position,
	            const std::string animation_path,
	            const time::time_t time = 0.0);

	/**
	 * Get the ID of the corresponding game entity.
	 *
	 * Accessing the game entity ID is thread-safe.
	 *
	 * @return Game entity ID.
	 */
	uint32_t get_id();

	/**
	 * Get the position of the entity inside the game world.
	 *
	 * Accessing the position curve REQUIRES a read lock on the render entity
	 * (using \p get_read_lock()) to ensure thread safety.
	 *
	 * @return Position curve of the entity.
	 */
	const curve::Continuous<coord::scene3> &get_position();

	/**
	 * Get the angle of the entity inside the game world.
	 *
	 * Accessing the angle curve REQUIRES a read lock on the render entity
	 * (using \p get_read_lock()) to ensure thread safety.
	 *
	 * @return Angle curve of the entity.
	 */
	const curve::Segmented<coord::phys_angle_t> &get_angle();

	/**
	 * Get the animation definition path.
	 *
	 * Accessing the animation path curve requires a read lock on the render entity
	 * (using \p get_read_lock()) to ensure thread safety.
	 *
	 * @return Path to the animation definition file.
	 */
	const curve::Discrete<std::string> &get_animation_path();

	/**
	 * Set how this entity should be drawn for the local observer.
	 *
	 * @param display  Visibility mode.
	 * @param ghost_position  Position for GHOST mode; ignored otherwise.
	 */
	void set_fog_display(fog_display_t display,
	                     const std::optional<coord::phys3> &ghost_position = std::nullopt);

	/**
	 * @return Current fog-of-war display mode.
	 */
	fog_display_t get_fog_display() const;

	/**
	 * @return Ghost position when display mode is GHOST.
	 */
	const std::optional<coord::phys3> &get_ghost_position() const;

private:
	/**
	 * ID of the game entity in the gamestate.
	 */
	uint32_t ref_id;

	/**
	 * Position inside the game world.
	 */
	curve::Continuous<coord::scene3> position;

	/**
	 * Angle of the entity inside the game world.
	 */
	curve::Segmented<coord::phys_angle_t> angle;

	/**
	 * Path to the animation definition file.
	 */
	curve::Discrete<std::string> animation_path;

	/**
	 * Fog-of-war display mode for the local observer.
	 */
	fog_display_t fog_display;

	/**
	 * Last-known world position used when \p fog_display is GHOST.
	 */
	std::optional<coord::phys3> ghost_position;
};
} // namespace openage::renderer::world
