// Copyright 2021-2023 the openage authors. See copying.md for legal info.

#include "time_loop.h"

#include <mutex>

#include "log/log.h"
#include "time/clock.h"


namespace openage::time {

TimeLoop::TimeLoop() :
	running{false},
	clock{std::make_shared<Clock>()} {}

TimeLoop::TimeLoop(const std::shared_ptr<Clock> clock) :
	running{false},
	clock{clock} {}

void TimeLoop::run() {
	this->start();
	while (this->running) {
		this->clock->update_time();
	}
	log::log(MSG(info) << "Time loop exited");
}

void TimeLoop::start() {
	std::unique_lock lock{this->mutex};

	this->running = true;

	if (this->clock->get_state() == ClockState::INIT) {
		// Start then immediately pause so the clock is ready but does not
		// advance until the menu (or headless entry) resumes it. Otherwise the
		// time-loop thread can race ahead of the presenter main menu.
		this->clock->start();
		this->clock->pause();
	}

	log::log(MSG(info) << "Time loop started");
}

void TimeLoop::stop() {
	std::unique_lock lock{this->mutex};

	this->running = false;

	log::log(MSG(info) << "Time loop stopped");
}

const std::shared_ptr<Clock> TimeLoop::get_clock() {
	std::shared_lock lock{this->mutex};

	return this->clock;
}

} // namespace openage::time
