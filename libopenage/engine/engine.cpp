// Copyright 2023-2026 the openage authors. See copying.md for legal info.

#include "engine.h"

#include "log/log.h"
#include "log/message.h"

#include "cvar/cvar.h"
#include "gamestate/simulation.h"
#include "presenter/presenter.h"
#include "time/time_loop.h"


namespace openage::engine {

Engine::Engine(mode mode,
               const util::Path &root_dir,
               const std::vector<std::string> &mods,
               const renderer::window_settings &window_settings) :
	running{true},
	run_mode{mode},
	root_dir{root_dir},
	threads{},
	window_settings{window_settings} {
	log::log(INFO
	         << "launching engine with root directory"
	         << root_dir);

	// read and apply the configuration files
	this->cvar_manager = std::make_shared<cvar::CVarManager>(this->root_dir["cfg"]);
	cvar_manager->load_all();

	// time loop
	this->time_loop = std::make_shared<time::TimeLoop>();

	// game simulation
	this->simulation = std::make_shared<gamestate::GameSimulation>(this->root_dir,
	                                                               this->cvar_manager,
	                                                               this->time_loop);
	this->simulation->set_modpacks(mods);

	// presenter (optional)
	if (this->run_mode == mode::FULL) {
		this->presenter = std::make_shared<presenter::Presenter>(this->root_dir,
		                                                         this->simulation,
		                                                         this->time_loop);
	}

	// spawn thread to run time loop
	this->threads.emplace_back([&]() {
		this->time_loop->run();

		this->time_loop.reset();
	});

#ifndef __APPLE__
	// On non-Apple platforms the presenter runs in a worker thread and the
	// simulation owns the main thread (see Engine::loop).
	if (this->run_mode == mode::FULL) {
		this->threads.emplace_back([&]() {
			this->presenter->run(this->window_settings);

			// Make sure that the presenter gets destructed in the same thread
			// otherwise OpenGL complains about missing contexts
			this->presenter.reset();
			this->running = false;
		});
	}
#endif

	log::log(INFO << "Using " << this->threads.size() + 1 << " threads "
	              << "(" << std::jthread::hardware_concurrency() << " available)");
}

void Engine::loop() {
#ifdef __APPLE__
	// Cocoa / Qt require GUI work on the process main thread on macOS
	// (including Apple Silicon). Run the simulation on a worker and keep the
	// presenter here so QML / OpenGL contexts stay valid.
	if (this->run_mode == mode::FULL && this->presenter) {
		this->threads.emplace_back([&]() {
			this->simulation->run();
			this->simulation.reset();
		});

		this->presenter->run(this->window_settings);
		this->presenter.reset();
		this->running = false;
		return;
	}
#endif

	// Run the main game simulation loop:
	this->simulation->run();

	// After stopping, clean up the simulation
	this->simulation.reset();
	if (this->run_mode != mode::FULL) {
		this->running = false;
	}
}

} // namespace openage::engine
