// Copyright 2026-2026 the openage authors. See copying.md for legal info.

#include "menu_controller.h"

#include "gamestate/game.h"
#include "gamestate/game_state.h"
#include "gamestate/player.h"
#include "gamestate/simulation.h"
#include "log/log.h"
#include "log/message.h"
#include "renderer/window.h"
#include "time/clock.h"
#include "time/time_loop.h"


namespace openage::presenter {

MenuController::MenuController(QObject *parent) :
	QObject{parent} {
	// Main menu holds simulation time until Start is chosen.
	this->is_paused = true;
}

void MenuController::set_window(const std::shared_ptr<renderer::Window> &window) {
	this->window = window;
}

void MenuController::set_time_loop(const std::shared_ptr<time::TimeLoop> &time_loop) {
	this->time_loop = time_loop;
}

void MenuController::set_simulation(const std::shared_ptr<gamestate::GameSimulation> &simulation) {
	this->simulation = simulation;
}

QString MenuController::screen() const {
	return this->current_screen;
}

void MenuController::set_screen(const QString &screen) {
	if (this->current_screen == screen) {
		return;
	}
	this->current_screen = screen;
	emit this->screenChanged();
}

bool MenuController::paused() const {
	return this->is_paused;
}

bool MenuController::in_game() const {
	return this->is_in_game;
}

bool MenuController::game_over() const {
	return this->is_game_over;
}

bool MenuController::has_winner() const {
	return this->winner_present;
}

QString MenuController::winner_label() const {
	if (not this->winner_present) {
		return QStringLiteral("No winner");
	}
	return QStringLiteral("Player %1").arg(this->winner_id);
}

qint64 MenuController::units_killed() const {
	return this->stat_units_killed;
}

qint64 MenuController::units_lost() const {
	return this->stat_units_lost;
}

qint64 MenuController::resources_gathered() const {
	return this->stat_resources_gathered;
}

double MenuController::apm() const {
	return this->stat_apm;
}

bool MenuController::blocks_game_input() const {
	return this->current_screen != QStringLiteral("game");
}

void MenuController::set_paused(bool paused) {
	if (this->is_paused == paused) {
		return;
	}
	this->is_paused = paused;
	emit this->pausedChanged();
}

void MenuController::clear_stats() {
	this->stat_units_killed = 0;
	this->stat_units_lost = 0;
	this->stat_resources_gathered = 0;
	this->stat_apm = 0.0;
	emit this->statsChanged();
}

void MenuController::startGame() {
	if (this->simulation) {
		auto game = this->simulation->get_game();
		if (game) {
			game->get_state()->clear_game_result();
		}
	}

	this->is_game_over = false;
	this->winner_present = false;
	this->winner_id = 0;
	emit this->gameOverChanged();
	this->clear_stats();

	this->is_in_game = true;
	emit this->inGameChanged();

	this->set_paused(false);
	if (this->time_loop) {
		auto clock = this->time_loop->get_clock();
		if (clock->get_state() == time::ClockState::PAUSED) {
			clock->resume();
		}
		else if (clock->get_state() == time::ClockState::INIT
		         || clock->get_state() == time::ClockState::STOPPED) {
			clock->start();
		}
	}

	this->set_screen(QStringLiteral("game"));
	log::log(MSG(info) << "Menu: starting game");
}

void MenuController::togglePause() {
	if (this->is_game_over) {
		// Escape on the summary screen returns to the main menu.
		this->quitToMenu();
		return;
	}

	if (not this->is_in_game) {
		return;
	}

	if (this->is_paused) {
		this->resumeGame();
		return;
	}

	this->set_paused(true);
	if (this->time_loop) {
		this->time_loop->get_clock()->pause();
	}
	this->set_screen(QStringLiteral("pause"));
	log::log(MSG(info) << "Menu: paused");
}

void MenuController::resumeGame() {
	if (not this->is_in_game || this->is_game_over) {
		return;
	}

	this->set_paused(false);
	if (this->time_loop) {
		this->time_loop->get_clock()->resume();
	}
	this->set_screen(QStringLiteral("game"));
	log::log(MSG(info) << "Menu: resumed");
}

void MenuController::quitToMenu() {
	this->set_paused(true);
	if (this->time_loop) {
		this->time_loop->get_clock()->pause();
	}

	if (this->simulation) {
		auto game = this->simulation->get_game();
		if (game) {
			game->get_state()->clear_game_result();
		}
	}

	this->is_in_game = false;
	emit this->inGameChanged();

	this->is_game_over = false;
	this->winner_present = false;
	emit this->gameOverChanged();
	this->clear_stats();

	this->set_screen(QStringLiteral("main"));
	log::log(MSG(info) << "Menu: returned to main menu");
}

void MenuController::quit() {
	log::log(MSG(info) << "Menu: quit requested");
	if (this->window) {
		this->window->close();
	}
}

void MenuController::apply_game_over(bool has_winner, gamestate::player_id_t winner_id) {
	if (this->is_game_over) {
		return;
	}

	this->is_game_over = true;
	this->winner_present = has_winner;
	this->winner_id = winner_id;
	emit this->gameOverChanged();

	this->set_paused(true);
	if (this->time_loop) {
		this->time_loop->get_clock()->pause();
	}

	this->refresh_stats();
	this->set_screen(QStringLiteral("gameover"));
	log::log(MSG(info) << "Menu: showing game-over summary");
}

void MenuController::refresh_stats() {
	this->stat_units_killed = 0;
	this->stat_units_lost = 0;
	this->stat_resources_gathered = 0;
	this->stat_apm = 0.0;

	if (not this->simulation || not this->time_loop) {
		emit this->statsChanged();
		return;
	}

	auto game = this->simulation->get_game();
	if (not game) {
		emit this->statsChanged();
		return;
	}

	auto state = game->get_state();
	auto clock = this->time_loop->get_clock();
	auto now = clock->get_time();
	double elapsed = now.to_double();
	if (elapsed <= 0.0) {
		elapsed = 1.0;
	}

	// Prefer the winner; otherwise the local view player; else lowest player id.
	std::shared_ptr<gamestate::Player> focus;
	if (this->winner_present && state->has_player(this->winner_id)) {
		focus = state->get_player(this->winner_id);
	}
	else if (state->has_player(state->get_view_player())) {
		focus = state->get_player(state->get_view_player());
	}
	else if (not state->get_players().empty()) {
		gamestate::player_id_t lowest = state->get_players().begin()->first;
		for (const auto &[pid, player] : state->get_players()) {
			(void)player;
			if (pid < lowest) {
				lowest = pid;
			}
		}
		focus = state->get_player(lowest);
	}

	if (focus) {
		this->stat_units_killed = focus->get_units_killed(now);
		this->stat_units_lost = focus->get_units_lost(now);
		this->stat_resources_gathered = focus->get_total_resources_gathered(now);
		this->stat_apm = focus->get_apm(now, elapsed);
	}

	emit this->statsChanged();
}

void MenuController::update() {
	if (not this->is_in_game || this->is_game_over || not this->simulation) {
		return;
	}

	auto game = this->simulation->get_game();
	if (not game) {
		return;
	}

	auto state = game->get_state();
	auto result = state->get_game_result();
	if (result.finished) {
		this->apply_game_over(result.has_winner, result.winner_id);
	}
}

} // namespace openage::presenter
