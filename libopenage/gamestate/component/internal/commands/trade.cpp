// Copyright 2026-2026 the openage authors. See copying.md for legal info.

#include "trade.h"


namespace openage::gamestate::component::command {

TradeCommand::TradeCommand(entity_id_t target_market,
                           entity_id_t home_market,
                           trade_state_t state) :
	target_market{target_market},
	home_market{home_market},
	state{state} {
}

entity_id_t TradeCommand::get_target_market() const {
	return this->target_market;
}

entity_id_t TradeCommand::get_home_market() const {
	return this->home_market;
}

trade_state_t TradeCommand::get_state() const {
	return this->state;
}

void TradeCommand::set_home_market(entity_id_t home) {
	this->home_market = home;
}

void TradeCommand::set_state(trade_state_t state) {
	this->state = state;
}

} // namespace openage::gamestate::component::command
