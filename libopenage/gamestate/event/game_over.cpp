// Copyright 2026-2026 the openage authors. See copying.md for legal info.

#include "game_over.h"

#include "gamestate/types.h"
#include "log/log.h"
#include "log/message.h"


namespace openage::gamestate::event {

// PlayerDefeatedHandler

PlayerDefeatedHandler::PlayerDefeatedHandler() :
	OnceEventHandler{"game.player_defeated"} {}

void PlayerDefeatedHandler::setup_event(const std::shared_ptr<openage::event::Event> & /* event */,
                                        const std::shared_ptr<openage::event::State> & /* state */) {}

void PlayerDefeatedHandler::invoke(openage::event::EventLoop & /* loop */,
                                   const std::shared_ptr<openage::event::EventEntity> & /* target */,
                                   const std::shared_ptr<openage::event::State> & /* state */,
                                   const time::time_t &time,
                                   const param_map &params) {
	auto player_id = params.get("player_id", player_id_t{0});
	log::log(MSG(info) << "[t=" << time << "] Player " << player_id << " defeated.");
}

time::time_t PlayerDefeatedHandler::predict_invoke_time(
	const std::shared_ptr<openage::event::EventEntity> & /* target */,
	const std::shared_ptr<openage::event::State> & /* state */,
	const time::time_t &at) {
	return at;
}


// GameOverHandler

GameOverHandler::GameOverHandler() :
	OnceEventHandler{"game.game_over"} {}

void GameOverHandler::setup_event(const std::shared_ptr<openage::event::Event> & /* event */,
                                  const std::shared_ptr<openage::event::State> & /* state */) {}

void GameOverHandler::invoke(openage::event::EventLoop & /* loop */,
                             const std::shared_ptr<openage::event::EventEntity> & /* target */,
                             const std::shared_ptr<openage::event::State> & /* state */,
                             const time::time_t &time,
                             const param_map &params) {
	auto has_winner = params.get("has_winner", true);
	if (has_winner) {
		auto winner_id = params.get("winner_id", player_id_t{0});
		log::log(MSG(info) << "[t=" << time << "] Game over — player " << winner_id << " wins!");
	}
	else {
		log::log(MSG(info) << "[t=" << time << "] Game over — no winner (all players defeated).");
	}
}

time::time_t GameOverHandler::predict_invoke_time(
	const std::shared_ptr<openage::event::EventEntity> & /* target */,
	const std::shared_ptr<openage::event::State> & /* state */,
	const time::time_t &at) {
	return at;
}

} // namespace openage::gamestate::event
