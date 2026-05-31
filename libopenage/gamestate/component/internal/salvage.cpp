// Copyright 2026-2026 the openage authors. See copying.md for legal info.

#include "salvage.h"

#include "event/event_loop.h"
#include "gamestate/component/types.h"


namespace openage::gamestate::component {

Salvage::Salvage(const std::shared_ptr<openage::event::EventLoop> &loop,
                 const std::string &resource_type,
                 int64_t starting_amount,
                 const time::time_t &creation_time) :
	resource_type{resource_type},
	amount_curve{loop, 0, "", nullptr, starting_amount},
	last_decay_time{creation_time} {
}

component_t Salvage::get_type() const {
	return component_t::SALVAGE;
}

const std::string &Salvage::get_resource_type() const {
	return this->resource_type;
}

int64_t Salvage::get_amount(const time::time_t &time) const {
	return this->amount_curve.get(time);
}

void Salvage::set_amount(const time::time_t &time, int64_t amount) {
	this->amount_curve.set_last(time, std::max(int64_t{0}, amount));
}

time::time_t Salvage::get_last_decay_time() const {
	return this->last_decay_time;
}

void Salvage::set_last_decay_time(const time::time_t &time) {
	this->last_decay_time = time;
}

} // namespace openage::gamestate::component
