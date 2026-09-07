// Copyright 2024-2024 the openage authors. See copying.md for legal info.

#pragma once

#include "gamestate/component/api_component.h"
#include "gamestate/component/types.h"


#include "curve/discrete.h"
#include "gamestate/definitions.h"

namespace openage::gamestate::component {

/**
 * Component for entities that can perform attacks.
 *
 * Wraps the nyan Attack ability object, which defines damage, range,
 * reload time, and any animated properties, and supports explicit curves
 * for tests and runtime modifiers.
 */
class Attack final : public APIComponent {
public:
	using APIComponent::APIComponent;

	component_t get_type() const override;

	int64_t get_damage(const time::time_t &time = time::TIME_MIN) const;
	void set_damage(const time::time_t &time, int64_t damage);
	void set_damage(std::shared_ptr<curve::Discrete<int64_t>> damage_curve);

	double get_reload_time(const time::time_t &time = time::TIME_MIN) const;
	void set_reload_time(const time::time_t &time, double reload_time);
	void set_reload_time(std::shared_ptr<curve::Discrete<double>> reload_curve);

	double get_max_range(const time::time_t &time = time::TIME_MIN) const;
	void set_max_range(const time::time_t &time, double max_range);
	void set_max_range(std::shared_ptr<curve::Discrete<double>> range_curve);

	double get_min_range(const time::time_t &time = time::TIME_MIN) const;
	void set_min_range(const time::time_t &time, double min_range);
	void set_min_range(std::shared_ptr<curve::Discrete<double>> min_range_curve);

	double get_blast_radius(const time::time_t &time = time::TIME_MIN) const;
	void set_blast_radius(const time::time_t &time, double blast_radius);
	void set_blast_radius(std::shared_ptr<curve::Discrete<double>> blast_curve);

	armor_class_t get_attack_type(const time::time_t &time = time::TIME_MIN) const;
	void set_attack_type(const time::time_t &time, armor_class_t type);

private:
	std::shared_ptr<curve::Discrete<int64_t>> damage;
	std::shared_ptr<curve::Discrete<double>> reload_time;
	std::shared_ptr<curve::Discrete<double>> max_range;
	std::shared_ptr<curve::Discrete<double>> min_range;
	std::shared_ptr<curve::Discrete<double>> blast_radius;
	std::shared_ptr<curve::Discrete<uint8_t>> attack_type;
};

} // namespace openage::gamestate::component
