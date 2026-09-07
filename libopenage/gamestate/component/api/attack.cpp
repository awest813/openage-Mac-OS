// Copyright 2024-2026 the openage authors. See copying.md for legal info.

#include "attack.h"

#include <nyan/nyan.h>

#include "gamestate/component/types.h"


namespace openage::gamestate::component {

component_t Attack::get_type() const {
	return component_t::ATTACK;
}

int64_t Attack::get_damage(const time::time_t &time) const {
	if (this->damage) {
		return this->damage->get(time);
	}
	try {
		const auto &ability = this->get_ability();
		auto dmg = ability.get<nyan::Int>("Attack.damage");
		if (dmg) {
			return dmg->get();
		}
	}
	catch (...) {
	}
	return 5;
}

void Attack::set_damage(const time::time_t &time, int64_t damage_val) {
	if (this->damage) {
		this->damage->set(time, damage_val);
	}
	else {
		this->damage = std::make_shared<curve::Discrete<int64_t>>(nullptr, time, "", nullptr, damage_val);
	}
}

void Attack::set_damage(std::shared_ptr<curve::Discrete<int64_t>> damage_curve) {
	this->damage = std::move(damage_curve);
}

double Attack::get_reload_time(const time::time_t &time) const {
	if (this->reload_time) {
		return this->reload_time->get(time);
	}
	try {
		const auto &ability = this->get_ability();
		auto reload = ability.get<nyan::Float>("Attack.reload_time");
		if (reload) {
			return reload->get();
		}
	}
	catch (...) {
	}
	return 2.0;
}

void Attack::set_reload_time(const time::time_t &time, double reload_val) {
	if (this->reload_time) {
		this->reload_time->set(time, reload_val);
	}
	else {
		this->reload_time = std::make_shared<curve::Discrete<double>>(nullptr, time, "", nullptr, reload_val);
	}
}

void Attack::set_reload_time(std::shared_ptr<curve::Discrete<double>> reload_curve) {
	this->reload_time = std::move(reload_curve);
}

double Attack::get_max_range(const time::time_t &time) const {
	if (this->max_range) {
		return this->max_range->get(time);
	}
	try {
		const auto &ability = this->get_ability();
		auto range = ability.get<nyan::Float>("Attack.max_range");
		if (range) {
			return range->get();
		}
	}
	catch (...) {
	}
	return 1.0;
}

void Attack::set_max_range(const time::time_t &time, double max_range_val) {
	if (this->max_range) {
		this->max_range->set(time, max_range_val);
	}
	else {
		this->max_range = std::make_shared<curve::Discrete<double>>(nullptr, time, "", nullptr, max_range_val);
	}
}

void Attack::set_max_range(std::shared_ptr<curve::Discrete<double>> range_curve) {
	this->max_range = std::move(range_curve);
}

double Attack::get_min_range(const time::time_t &time) const {
	if (this->min_range) {
		return this->min_range->get(time);
	}
	try {
		const auto &ability = this->get_ability();
		auto range = ability.get<nyan::Float>("Attack.min_range");
		if (range) {
			return range->get();
		}
	}
	catch (...) {
	}
	return 0.0;
}

void Attack::set_min_range(const time::time_t &time, double min_range_val) {
	if (this->min_range) {
		this->min_range->set(time, min_range_val);
	}
	else {
		this->min_range = std::make_shared<curve::Discrete<double>>(nullptr, time, "", nullptr, min_range_val);
	}
}

void Attack::set_min_range(std::shared_ptr<curve::Discrete<double>> min_range_curve) {
	this->min_range = std::move(min_range_curve);
}

double Attack::get_blast_radius(const time::time_t &time) const {
	if (this->blast_radius) {
		return this->blast_radius->get(time);
	}
	try {
		const auto &ability = this->get_ability();
		auto radius = ability.get<nyan::Float>("Attack.blast_radius");
		if (radius) {
			return radius->get();
		}
	}
	catch (...) {
	}
	return 0.0;
}

void Attack::set_blast_radius(const time::time_t &time, double blast_radius_val) {
	if (this->blast_radius) {
		this->blast_radius->set(time, blast_radius_val);
	}
	else {
		this->blast_radius = std::make_shared<curve::Discrete<double>>(nullptr, time, "", nullptr, blast_radius_val);
	}
}

void Attack::set_blast_radius(std::shared_ptr<curve::Discrete<double>> blast_curve) {
	this->blast_radius = std::move(blast_curve);
}

armor_class_t Attack::get_attack_type(const time::time_t &time) const {
	if (this->attack_type) {
		return static_cast<armor_class_t>(this->attack_type->get(time));
	}
	// Defaults to Pierce if ranged (> 2 tiles), else Melee
	if (this->get_max_range(time) > 2.0) {
		return armor_class_t::PIERCE;
	}
	return armor_class_t::MELEE;
}

void Attack::set_attack_type(const time::time_t &time, armor_class_t type) {
	uint8_t val = static_cast<uint8_t>(type);
	if (this->attack_type) {
		this->attack_type->set(time, val);
	}
	else {
		this->attack_type = std::make_shared<curve::Discrete<uint8_t>>(nullptr, time, "", nullptr, val);
	}
}

} // namespace openage::gamestate::component
