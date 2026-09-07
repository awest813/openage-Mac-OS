// Copyright 2026-2026 the openage authors. See copying.md for legal info.

#pragma once

#include <cstdint>

#include "gamestate/component/internal/commands/base_command.h"
#include "gamestate/component/internal/commands/types.h"
#include "gamestate/types.h"


namespace openage::gamestate::component::command {

/**
 * Stage of the trade route.
 */
enum class trade_state_t : uint8_t {
	TO_TARGET,
	RETURNING_HOME,
};

/**
 * Command for a trade unit (Trade Cart or Trade Cog) to run a continuous trade route.
 */
class TradeCommand : public Command {
public:
	explicit TradeCommand(entity_id_t target_market,
	                     entity_id_t home_market = 0,
	                     trade_state_t state = trade_state_t::TO_TARGET);
	~TradeCommand() override = default;

	command_t get_type() const override {
		return command_t::TRADE;
	}

	entity_id_t get_target_market() const;
	entity_id_t get_home_market() const;
	trade_state_t get_state() const;

	void set_home_market(entity_id_t home);
	void set_state(trade_state_t state);

private:
	entity_id_t target_market;
	entity_id_t home_market;
	trade_state_t state;
};

} // namespace openage::gamestate::component::command
