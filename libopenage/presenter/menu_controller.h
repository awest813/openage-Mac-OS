// Copyright 2026-2026 the openage authors. See copying.md for legal info.

#pragma once

#include <memory>
#include <string>

#include <QObject>
#include <QString>

#include "gamestate/types.h"


namespace openage {

namespace gamestate {
class GameSimulation;
} // namespace gamestate

namespace renderer {
class Window;
} // namespace renderer

namespace time {
class Clock;
class TimeLoop;
} // namespace time

namespace presenter {

/**
 * Bridge between the QML menu shell and engine subsystems.
 *
 * Exposed to QML as the context property `menuController`.
 */
class MenuController : public QObject {
	Q_OBJECT

	Q_PROPERTY(QString screen READ screen NOTIFY screenChanged)
	Q_PROPERTY(bool paused READ paused NOTIFY pausedChanged)
	Q_PROPERTY(bool inGame READ in_game NOTIFY inGameChanged)
	Q_PROPERTY(bool gameOver READ game_over NOTIFY gameOverChanged)
	Q_PROPERTY(bool hasWinner READ has_winner NOTIFY gameOverChanged)
	Q_PROPERTY(QString winnerLabel READ winner_label NOTIFY gameOverChanged)
	Q_PROPERTY(qint64 unitsKilled READ units_killed NOTIFY statsChanged)
	Q_PROPERTY(qint64 unitsLost READ units_lost NOTIFY statsChanged)
	Q_PROPERTY(qint64 resourcesGathered READ resources_gathered NOTIFY statsChanged)
	Q_PROPERTY(double apm READ apm NOTIFY statsChanged)
	Q_PROPERTY(bool blocksGameInput READ blocks_game_input NOTIFY screenChanged)

public:
	MenuController(QObject *parent = nullptr);
	~MenuController() override = default;

	void set_window(const std::shared_ptr<renderer::Window> &window);
	void set_time_loop(const std::shared_ptr<time::TimeLoop> &time_loop);
	void set_simulation(const std::shared_ptr<gamestate::GameSimulation> &simulation);

	/**
	 * Poll simulation for game-over and refresh end-game stats.
	 * Called once per presenter frame.
	 */
	void update();

	QString screen() const;
	void set_screen(const QString &screen);

	bool paused() const;
	bool in_game() const;
	bool game_over() const;
	bool has_winner() const;
	QString winner_label() const;
	qint64 units_killed() const;
	qint64 units_lost() const;
	qint64 resources_gathered() const;
	double apm() const;

	/**
	 * True when the active menu screen should absorb game/camera input.
	 */
	bool blocks_game_input() const;

public slots:
	/** Leave the main menu and enter the running game screen. */
	void startGame();

	/** Toggle pause menu while a game is running. */
	void togglePause();

	/** Resume from the pause menu. */
	void resumeGame();

	/** Return to the main menu (pauses simulation clock). */
	void quitToMenu();

	/** Close the application window. */
	void quit();

signals:
	void screenChanged();
	void pausedChanged();
	void inGameChanged();
	void gameOverChanged();
	void statsChanged();

private:
	void set_paused(bool paused);
	void refresh_stats();
	void apply_game_over(bool has_winner, gamestate::player_id_t winner_id);

	std::shared_ptr<renderer::Window> window;
	std::shared_ptr<time::TimeLoop> time_loop;
	std::shared_ptr<gamestate::GameSimulation> simulation;

	QString current_screen{"main"};
	bool is_paused{false};
	bool is_in_game{false};
	bool is_game_over{false};
	bool winner_present{false};
	gamestate::player_id_t winner_id{0};

	qint64 stat_units_killed{0};
	qint64 stat_units_lost{0};
	qint64 stat_resources_gathered{0};
	double stat_apm{0.0};
};

} // namespace presenter
} // namespace openage
