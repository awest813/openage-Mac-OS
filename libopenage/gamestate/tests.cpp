// Copyright 2025-2025 the openage authors. See copying.md for legal info.

#include "../testing/testing.h"

#include <memory>
#include <string>
#include <vector>

#include <nyan/nyan.h>

#include "event/eventhandler.h"
#include "event/event_loop.h"
#include "activity/condition/next_command.h"
#include "component/internal/command_queue.h"
#include "component/internal/commands/attack.h"
#include "component/internal/commands/gather.h"
#include "component/internal/commands/idle.h"
#include "component/internal/commands/train.h"
#include "component/internal/ownership.h"
#include "event/send_command.h"
#include "game_entity.h"
#include "game_state.h"
#include "player.h"
#include "time/time.h"


namespace openage::gamestate::tests {

namespace {

std::shared_ptr<GameEntity> make_entity_with_command_queue(const std::shared_ptr<event::EventLoop> &loop,
                                                           entity_id_t id) {
	auto entity = std::make_shared<GameEntity>(id);
	entity->add_component(std::make_shared<component::CommandQueue>(loop));

	return entity;
}

} // namespace

void player_resources() {
	auto loop = std::make_shared<event::EventLoop>();
	auto db = nyan::Database::create();
	auto view = db->new_view();
	Player player{0, view, loop};

	const nyan::fqon_t resource{"test.resource.Food"};
	auto t0 = time::time_t::from_int(0);
	auto t1 = time::time_t::from_int(1);

	TESTEQUALS(player.get_resource(t0, resource), 0);

	player.add_resource(t0, resource, 25);
	TESTEQUALS(player.get_resource(t0, resource), 25);

	player.add_resource(t1, resource, -10);
	TESTEQUALS(player.get_resource(t1, resource), 15);
	TESTEQUALS(player.get_resource(t0, resource), 25);
}

void next_command_conditions() {
	auto loop = std::make_shared<event::EventLoop>();
	auto entity = make_entity_with_command_queue(loop, 0);
	auto command_queue = std::dynamic_pointer_cast<component::CommandQueue>(
		entity->get_component(component::component_t::COMMANDQUEUE));
	auto t0 = time::time_t::from_int(0);

	command_queue->add_command(t0, std::make_shared<component::command::IdleCommand>());
	TESTEQUALS(activity::next_command_idle(t0, entity), true);
	TESTEQUALS(activity::next_command_move(t0, entity), false);
	TESTEQUALS(activity::next_command_attack(t0, entity), false);
	TESTEQUALS(activity::next_command_gather(t0, entity), false);
	TESTEQUALS(activity::next_command_train(t0, entity), false);
}

void send_command_variants() {
	auto loop = std::make_shared<event::EventLoop>();
	auto db = nyan::Database::create();
	auto state = std::make_shared<GameState>(db, loop);
	auto entity = make_entity_with_command_queue(loop, 7);
	auto t0 = time::time_t::from_int(0);

	state->add_game_entity(entity);

	event::SendCommandHandler handler;

	handler.invoke(*loop,
	               nullptr,
	               state,
	               t0,
	               event::EventHandler::param_map{
	                   {"type", component::command::command_t::ATTACK},
	                   {"target_entity_id", entity_id_t{23}},
	                   {"entity_ids", std::vector<entity_id_t>{7}},
	               });

	auto command_queue = std::dynamic_pointer_cast<component::CommandQueue>(
		entity->get_component(component::component_t::COMMANDQUEUE));
	auto attack_command = std::dynamic_pointer_cast<component::command::AttackCommand>(
		command_queue->pop_command(t0));
	TESTEQUALS(attack_command != nullptr, true);
	TESTEQUALS(attack_command->get_target(), 23);

	handler.invoke(*loop,
	               nullptr,
	               state,
	               t0,
	               event::EventHandler::param_map{
	                   {"type", component::command::command_t::GATHER},
	                   {"target_entity_id", entity_id_t{42}},
	                   {"entity_ids", std::vector<entity_id_t>{7}},
	               });

	auto gather_command = std::dynamic_pointer_cast<component::command::GatherCommand>(
		command_queue->pop_command(t0));
	TESTEQUALS(gather_command != nullptr, true);
	TESTEQUALS(gather_command->get_target(), 42);

	handler.invoke(*loop,
	               nullptr,
	               state,
	               t0,
	               event::EventHandler::param_map{
	                   {"type", component::command::command_t::TRAIN},
	                   {"game_entity", std::string{"test.unit.Spearman"}},
	                   {"entity_ids", std::vector<entity_id_t>{7}},
	               });

	auto train_command = std::dynamic_pointer_cast<component::command::TrainCommand>(
		command_queue->pop_command(t0));
	TESTEQUALS(train_command != nullptr, true);
	TESTEQUALS(train_command->get_game_entity(), std::string{"test.unit.Spearman"});
}

void production_requests() {
	auto loop = std::make_shared<event::EventLoop>();
	auto db = nyan::Database::create();
	auto state = std::make_shared<GameState>(db, loop);

	auto t0 = time::time_t::from_int(0);
	auto t5 = time::time_t::from_int(5);
	auto t10 = time::time_t::from_int(10);

	TESTEQUALS(state->pending_production_count(), 0);

	// Two units queued: one ready at t=5, one at t=10.
	state->request_production(0, "test.unit.Spearman", t5);
	state->request_production(0, "test.unit.Archer", t10);
	TESTEQUALS(state->pending_production_count(), 2);

	// Nothing completed before the first completion time.
	auto completed_early = state->take_completed_productions(t0);
	TESTEQUALS(completed_early.size(), 0);
	TESTEQUALS(state->pending_production_count(), 2);

	// Only the first unit has finished at t=5.
	auto completed_mid = state->take_completed_productions(t5);
	TESTEQUALS(completed_mid.size(), 1);
	TESTEQUALS(completed_mid.at(0).game_entity, std::string{"test.unit.Spearman"});
	TESTEQUALS(state->pending_production_count(), 1);

	// The second unit finishes by t=10 and is drained.
	auto completed_late = state->take_completed_productions(t10);
	TESTEQUALS(completed_late.size(), 1);
	TESTEQUALS(completed_late.at(0).game_entity, std::string{"test.unit.Archer"});
	TESTEQUALS(state->pending_production_count(), 0);
}

void player_state_transitions() {
	auto loop = std::make_shared<event::EventLoop>();
	auto db = nyan::Database::create();
	auto view = db->new_view();
	Player player{0, view, loop};

	TESTEQUALS(player.get_state() == player_state_t::ALIVE, true);

	player.set_state(player_state_t::DEFEATED);
	TESTEQUALS(player.get_state() == player_state_t::DEFEATED, true);

	player.set_state(player_state_t::WINNER);
	TESTEQUALS(player.get_state() == player_state_t::WINNER, true);
}

// Helper: create a building entity (has OWNERSHIP, no MOVE) and add it to state.
static std::shared_ptr<GameEntity>
make_building(entity_id_t id,
              player_id_t owner,
              const std::shared_ptr<event::EventLoop> &loop,
              const std::shared_ptr<GameState> &state,
              const time::time_t &t) {
	auto entity = std::make_shared<GameEntity>(id);
	auto ownership = std::make_shared<component::Ownership>(loop);
	ownership->set_owner(t, owner);
	entity->add_component(ownership);
	state->add_game_entity(entity);
	return entity;
}

void player_defeated_on_last_building_destroyed() {
	auto loop = std::make_shared<event::EventLoop>();
	auto db = nyan::Database::create();
	auto state = std::make_shared<GameState>(db, loop);

	auto view = db->new_view();
	auto p0 = std::make_shared<Player>(0, view, loop);
	auto p1 = std::make_shared<Player>(1, view, loop);
	state->add_player(p0);
	state->add_player(p1);

	auto t0 = time::time_t::from_int(0);

	// Two buildings for player 0, one for player 1.
	make_building(10, 0, loop, state, t0);
	make_building(11, 0, loop, state, t0);
	make_building(20, 1, loop, state, t0);

	// Destroy one building of player 0 — still has one left, not defeated.
	state->remove_game_entity(10, t0);
	TESTEQUALS(p0->get_state() == player_state_t::ALIVE, true);

	// Destroy player 0's last building — now defeated; player 1 wins.
	state->remove_game_entity(11, t0);
	TESTEQUALS(p0->get_state() == player_state_t::DEFEATED, true);
	TESTEQUALS(p1->get_state() == player_state_t::WINNER, true);
	TESTEQUALS(state->get_alive_player_count(), 0);
}

void no_defeat_for_unit_death() {
	auto loop = std::make_shared<event::EventLoop>();
	auto db = nyan::Database::create();
	auto state = std::make_shared<GameState>(db, loop);

	auto view = db->new_view();
	auto p0 = std::make_shared<Player>(0, view, loop);
	state->add_player(p0);

	auto t0 = time::time_t::from_int(0);

	// One building (owned, no MOVE) and one unit-like entity (owned, no special
	// flag — still treated as non-building via the plain remove_game_entity(id)).
	make_building(10, 0, loop, state, t0);

	// Add a second entity owned by player 0 but remove it via the no-time
	// overload (resource depletion path) — player must not be defeated.
	auto unit = std::make_shared<GameEntity>(99);
	auto ownership = std::make_shared<component::Ownership>(loop);
	ownership->set_owner(t0, 0);
	unit->add_component(ownership);
	state->add_game_entity(unit);

	state->remove_game_entity(99); // no defeat check
	TESTEQUALS(p0->get_state() == player_state_t::ALIVE, true);

	// Building still alive.
	state->remove_game_entity(10, t0);
	TESTEQUALS(p0->get_state() == player_state_t::DEFEATED, true);
}

} // namespace openage::gamestate::tests
