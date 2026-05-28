// Copyright 2026-2026 the openage authors. See copying.md for legal info.

#include "stance.h"

#include "gamestate/component/types.h"


namespace openage::gamestate::component {

Stance::Stance(const std::shared_ptr<openage::event::EventLoop> &loop) :
	stance_curve(loop, 0) {
	this->stance_curve.set_insert(time::time_t::from_int(0), stance_t::AGGRESSIVE);
}

inline component_t Stance::get_type() const {
	return component_t::STANCE;
}

curve::Discrete<stance_t> &Stance::get_stances() {
	return this->stance_curve;
}

stance_t Stance::get_stance(const time::time_t &time) const {
	return this->stance_curve.get(time);
}

void Stance::set_stance(const time::time_t &time, stance_t stance) {
	this->stance_curve.set_last(time, stance);
}

} // namespace openage::gamestate::component
