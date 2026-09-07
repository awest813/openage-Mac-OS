// Copyright 2026-2026 the openage authors. See copying.md for legal info.

#include "research_complete.h"

#include "gamestate/game_state.h"
#include "log/log.h"
#include "log/message.h"


namespace openage::gamestate::event {

ResearchCompleteHandler::ResearchCompleteHandler() :
	OnceEventHandler{"game.complete_research"} {
}

void ResearchCompleteHandler::setup_event(const std::shared_ptr<openage::event::Event> & /* event */,
                                         const std::shared_ptr<openage::event::State> & /* state */) {
}

void ResearchCompleteHandler::invoke(openage::event::EventLoop & /* loop */,
                                    const std::shared_ptr<openage::event::EventEntity> & /* target */,
                                    const std::shared_ptr<openage::event::State> &state,
                                    const time::time_t &time,
                                    const param_map &params) {
	auto gstate = std::dynamic_pointer_cast<gamestate::GameState>(state);
	if (!gstate) {
		return;
	}

	auto building_id = params.get("building_id", gamestate::entity_id_t{0});
	if (building_id != 0) {
		gstate->complete_research(building_id, time);
		log::log(MSG(info) << "Research completed for building " << building_id);
	}
}

time::time_t ResearchCompleteHandler::predict_invoke_time(
    const std::shared_ptr<openage::event::EventEntity> & /* target */,
    const std::shared_ptr<openage::event::State> & /* state */,
    const time::time_t &at) {
	return at;
}

} // namespace openage::gamestate::event
