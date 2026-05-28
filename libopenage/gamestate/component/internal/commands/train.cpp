// Copyright 2026-2026 the openage authors. See copying.md for legal info.

#include "train.h"


namespace openage::gamestate::component::command {

TrainCommand::TrainCommand(const std::string &game_entity) :
	game_entity{game_entity} {}

const std::string &TrainCommand::get_game_entity() const {
	return this->game_entity;
}

} // namespace openage::gamestate::component::command
