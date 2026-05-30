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

void Player::set_id(entity_id_t id) {
	this->id = id;
}

} // namespace openage::gamestate
