// Copyright 2026-2026 the openage authors. See copying.md for legal info.

#include "research.h"


namespace openage::gamestate::component::command {

ResearchCommand::ResearchCommand(int64_t tech_id) :
	tech_id{tech_id} {}

int64_t ResearchCommand::get_tech_id() const {
	return this->tech_id;
}

} // namespace openage::gamestate::component::command
