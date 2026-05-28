// Copyright 2018-2024 the openage authors. See copying.md for legal info.

#include "player.h"

#include "curve/discrete.h"
#include "event/event_loop.h"
#include "nyan/nyan.h"


namespace openage::gamestate {

Player::Player(player_id_t id,
               const std::shared_ptr<nyan::View> &db_view,
               const std::shared_ptr<openage::event::EventLoop> &loop) :
	id{id},
	db_view{db_view},
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
