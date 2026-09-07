// Copyright 2018-2026 the openage authors. See copying.md for legal info.

#include "player.h"

#include <algorithm>

#include "curve/discrete.h"
#include "event/event_loop.h"
#include "gamestate/definitions.h"
#include "nyan/nyan.h"


namespace openage::gamestate {

Player::Player(player_id_t id,
               const std::shared_ptr<nyan::View> &db_view,
               const std::shared_ptr<openage::event::EventLoop> &loop) :
	id{id},
	db_view{db_view},
	population_demand{std::make_shared<curve::Discrete<int64_t>>(loop, 0, "", nullptr, 0)},
	population_capacity{std::make_shared<curve::Discrete<int64_t>>(loop, 0, "", nullptr, 0)},
	units_killed{std::make_shared<curve::Discrete<int64_t>>(loop, 0, "", nullptr, 0)},
	units_lost{std::make_shared<curve::Discrete<int64_t>>(loop, 0, "", nullptr, 0)},
	trade_profit{std::make_shared<curve::Discrete<int64_t>>(loop, 0, "", nullptr, 0)},
	actions_issued{std::make_shared<curve::Discrete<int64_t>>(loop, 0, "", nullptr, 0)},
	current_age{std::make_shared<curve::Discrete<int64_t>>(loop, 0, "", nullptr, static_cast<int64_t>(age_t::DARK_AGE))},
	loop{loop} {
}

std::shared_ptr<Player> Player::copy(entity_id_t id) {
	auto copy = std::shared_ptr<Player>(new Player(*this));
	copy->set_id(id);

	return copy;
}

player_id_t Player::get_id() const {
	return this->id;
}

const std::shared_ptr<nyan::View> &Player::get_db_view() const {
	return this->db_view;
}

void Player::init_resource(const time::time_t &time,
                           const std::shared_ptr<openage::event::EventLoop> &loop,
                           const nyan::fqon_t &resource,
                           int64_t amount) {
	if (this->resources.contains(resource)) {
		return;
	}
	this->resources.emplace(resource,
	                        std::make_shared<curve::Discrete<int64_t>>(loop, 0, "", nullptr, amount));
	this->resources.at(resource)->set_last(time, amount);
}

int64_t Player::get_resource(const time::time_t &time, const nyan::fqon_t &resource) const {
	auto it = this->resources.find(resource);
	if (it == this->resources.end()) {
		return 0;
	}
	return it->second->get(time);
}

void Player::add_resource(const time::time_t &time,
                          const nyan::fqon_t &resource,
                          int64_t amount) {
	auto it = this->resources.find(resource);
	if (it == this->resources.end()) {
		this->init_resource(time, this->loop, resource, 0);
		it = this->resources.find(resource);
	}
	int64_t current = it->second->get(time);
	it->second->set_last(time, current + amount);
}

void Player::init_population(const time::time_t &time, int64_t capacity) {
	this->population_capacity->set_last(time, std::max(int64_t{0}, capacity));
}

int64_t Player::get_population_demand(const time::time_t &time) const {
	return this->population_demand->get(time);
}

int64_t Player::get_population_capacity(const time::time_t &time) const {
	return std::min(this->population_capacity->get(time), POPULATION_MAX);
}

int64_t Player::get_population_space(const time::time_t &time) const {
	return std::max(int64_t{0},
	                this->get_population_capacity(time) - this->get_population_demand(time));
}

bool Player::has_population_space(const time::time_t &time, int64_t amount) const {
	return this->get_population_demand(time) + amount <= this->get_population_capacity(time);
}

void Player::add_population_demand(const time::time_t &time, int64_t amount) {
	int64_t current = this->population_demand->get(time);
	this->population_demand->set_last(time, std::max(int64_t{0}, current + amount));
}

void Player::add_population_capacity(const time::time_t &time, int64_t amount) {
	int64_t current = this->population_capacity->get(time);
	this->population_capacity->set_last(time, std::max(int64_t{0}, current + amount));
}

player_state_t Player::get_state() const {
	return this->state;
}

void Player::set_state(player_state_t new_state) {
	this->state = new_state;
}

void Player::record_kill(const time::time_t &time, int64_t amount) {
	int64_t current = this->units_killed->get(time);
	this->units_killed->set_last(time, current + amount);
}

int64_t Player::get_units_killed(const time::time_t &time) const {
	return this->units_killed->get(time);
}

void Player::record_loss(const time::time_t &time, int64_t amount) {
	int64_t current = this->units_lost->get(time);
	this->units_lost->set_last(time, current + amount);
}

int64_t Player::get_units_lost(const time::time_t &time) const {
	return this->units_lost->get(time);
}

void Player::record_resource_gathered(const time::time_t &time,
                                      const nyan::fqon_t &resource,
                                      int64_t amount) {
	auto it = this->resources_gathered.find(resource);
	if (it == this->resources_gathered.end()) {
		it = this->resources_gathered.emplace(
			resource,
			std::make_shared<curve::Discrete<int64_t>>(this->loop, 0, "", nullptr, 0)).first;
	}
	int64_t current = it->second->get(time);
	it->second->set_last(time, current + amount);
}

int64_t Player::get_resource_gathered(const time::time_t &time,
                                      const nyan::fqon_t &resource) const {
	auto it = this->resources_gathered.find(resource);
	if (it == this->resources_gathered.end()) {
		return 0;
	}
	return it->second->get(time);
}

int64_t Player::get_total_resources_gathered(const time::time_t &time) const {
	int64_t total = 0;
	for (const auto &[resource, curve] : this->resources_gathered) {
		total += curve->get(time);
	}
	return total;
}

void Player::record_tribute_sent(const time::time_t &time,
                                 const nyan::fqon_t &resource,
                                 int64_t amount) {
	auto it = this->tribute_sent.find(resource);
	if (it == this->tribute_sent.end()) {
		it = this->tribute_sent.emplace(
			resource,
			std::make_shared<curve::Discrete<int64_t>>(this->loop, 0, "", nullptr, 0)).first;
	}
	int64_t current = it->second->get(time);
	it->second->set_last(time, current + amount);
}

int64_t Player::get_tribute_sent(const time::time_t &time,
                                 const nyan::fqon_t &resource) const {
	auto it = this->tribute_sent.find(resource);
	if (it == this->tribute_sent.end()) {
		return 0;
	}
	return it->second->get(time);
}

void Player::record_tribute_received(const time::time_t &time,
                                     const nyan::fqon_t &resource,
                                     int64_t amount) {
	auto it = this->tribute_received.find(resource);
	if (it == this->tribute_received.end()) {
		it = this->tribute_received.emplace(
			resource,
			std::make_shared<curve::Discrete<int64_t>>(this->loop, 0, "", nullptr, 0)).first;
	}
	int64_t current = it->second->get(time);
	it->second->set_last(time, current + amount);
}

int64_t Player::get_tribute_received(const time::time_t &time,
                                     const nyan::fqon_t &resource) const {
	auto it = this->tribute_received.find(resource);
	if (it == this->tribute_received.end()) {
		return 0;
	}
	return it->second->get(time);
}

void Player::record_trade_profit(const time::time_t &time, int64_t amount) {
	int64_t current = this->trade_profit->get(time);
	this->trade_profit->set_last(time, current + amount);
}

int64_t Player::get_trade_profit(const time::time_t &time) const {
	return this->trade_profit->get(time);
}

void Player::record_action(const time::time_t &time, int64_t amount) {
	int64_t current = this->actions_issued->get(time);
	this->actions_issued->set_last(time, current + amount);
}

int64_t Player::get_actions_issued(const time::time_t &time) const {
	return this->actions_issued->get(time);
}

double Player::get_apm(const time::time_t &time, double elapsed_seconds) const {
	if (elapsed_seconds <= 0) {
		return 0.0;
	}
	return static_cast<double>(this->get_actions_issued(time)) / (elapsed_seconds / 60.0);
}

age_t Player::get_age(const time::time_t &time) const {
	return static_cast<age_t>(this->current_age->get(time));
}

void Player::set_age(const time::time_t &time, age_t age) {
	this->current_age->set_last(time, static_cast<int64_t>(age));
}

bool Player::has_researched(int64_t tech_id) const {
	return this->researched_techs.contains(tech_id);
}

void Player::mark_researched(int64_t tech_id) {
	this->researched_techs.insert(tech_id);
}

int64_t Player::get_tech_count() const {
	return static_cast<int64_t>(this->researched_techs.size());
}

void Player::set_id(entity_id_t id) {
	this->id = id;
}

} // namespace openage::gamestate
