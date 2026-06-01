// Copyright 2026-2026 the openage authors. See copying.md for legal info.

#include "deconstruct_complete.h"

#include "coord/phys.h"
#include "gamestate/api/creatable.h"
#include "gamestate/definitions.h"
#include "gamestate/game_state.h"
#include "log/log.h"
#include "log/message.h"


namespace openage::gamestate::event {

DeconstructCompleteHandler::DeconstructCompleteHandler() :
	OnceEventHandler{"game.deconstruct_complete"} {
}

void DeconstructCompleteHandler::setup_event(const std::shared_ptr<openage::event::Event> & /* event */,
                                             const std::shared_ptr<openage::event::State> & /* state */) {
}

void DeconstructCompleteHandler::invoke(openage::event::EventLoop & /* loop */,
                                        const std::shared_ptr<openage::event::EventEntity> & /* target */,
                                        const std::shared_ptr<openage::event::State> &state,
                                        const time::time_t &time,
                                        const param_map &params) {
	auto gstate = std::dynamic_pointer_cast<gamestate::GameState>(state);

	auto building_id = params.get("building_id", gamestate::entity_id_t{0});
	auto building_pos = params.get("building_pos", coord::phys3{0, 0, 0});
	auto recovery_fraction = params.get("recovery_fraction", gamestate::DECONSTRUCT_RECOVERY_FRACTION);

	auto cost = api::building_cost_from_event_params(params);
	if (not cost.has_value() || cost->empty()) {
		log::log(MSG(warn) << "DeconstructComplete: missing cost data for building " << building_id << ".");
		return;
	}

	gstate->complete_deconstruction(building_id, building_pos, cost.value(), recovery_fraction, time);

	log::log(MSG(info) << "Deconstruction of building " << building_id << " completed.");
}

time::time_t DeconstructCompleteHandler::predict_invoke_time(
    const std::shared_ptr<openage::event::EventEntity> & /* target */,
    const std::shared_ptr<openage::event::State> & /* state */,
    const time::time_t &at) {
	return at;
}

} // namespace openage::gamestate::event
