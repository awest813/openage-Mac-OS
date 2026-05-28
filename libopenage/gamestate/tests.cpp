// Copyright 2025-2026 the openage authors. See copying.md for legal info.

#include "../testing/testing.h"

#include <memory>
#include <string>
#include <vector>

#include <nyan/nyan.h>

#include "coord/phys.h"
#include "event/eventhandler.h"
#include "event/event_loop.h"
#include "activity/condition/next_command.h"
#include "component/internal/command_queue.h"
#include "component/internal/commands/attack.h"
#include "component/internal/commands/attack_move.h"
#include "component/internal/commands/gather.h"
#include "component/internal/commands/guard.h"
#include "component/internal/commands/idle.h"
#include "component/internal/commands/patrol.h"
#include "component/internal/commands/train.h"
#include "component/internal/ownership.h"
#include "component/internal/stance.h"
#include "event/game_over.h"
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
	TESTEQUALS(activity::next_command_attack_move(t0, entity), false);
	TESTEQUALS(activity::next_command_patrol(t0, entity), false);
	TESTEQUALS(activity::next_command_guard(t0, entity), false);
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

void carried_resources_lifecycle() {
	auto loop = std::make_shared<event::EventLoop>();
	auto db = nyan::Database::create();
	auto state = std::make_shared<GameState>(db, loop);

	auto t0 = time::time_t::from_int(0);

	// A gatherer entity exists in the state.
	auto gatherer = std::make_shared<GameEntity>(5);
	state->add_game_entity(gatherer);

	TESTEQUALS(state->is_carrying_resources(5), false);
	TESTEQUALS(state->get_carried_resource(5).has_value(), false);

	state->set_carried_resource(5, "test.resource.Food", 12);
	TESTEQUALS(state->is_carrying_resources(5), true);
	auto cargo = state->get_carried_resource(5);
	TESTEQUALS(cargo.has_value(), true);
	TESTEQUALS(cargo->resource_type, std::string{"test.resource.Food"});
	TESTEQUALS(cargo->amount, 12);

	// Explicit clear (drop-off path).
	state->clear_carried_resource(5);
	TESTEQUALS(state->is_carrying_resources(5), false);

	// Cargo is cleaned up when the carrying entity is destroyed (leak fix).
	state->set_carried_resource(5, "test.resource.Wood", 7);
	TESTEQUALS(state->is_carrying_resources(5), true);
	state->remove_game_entity(5, t0);
	TESTEQUALS(state->is_carrying_resources(5), false);
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

	// check_defeat fires events via the loop; register their handlers.
	loop->add_event_handler(std::make_shared<event::PlayerDefeatedHandler>());
	loop->add_event_handler(std::make_shared<event::GameOverHandler>());

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

	// check_defeat fires events via the loop; register their handlers.
	loop->add_event_handler(std::make_shared<event::PlayerDefeatedHandler>());
	loop->add_event_handler(std::make_shared<event::GameOverHandler>());

	auto view = db->new_view();
	auto p0 = std::make_shared<Player>(0, view, loop);
	state->add_player(p0);

	auto t0 = time::time_t::from_int(0);

	// One owned building for player 0.
	make_building(10, 0, loop, state, t0);

	// A second owned entity removed via the no-time overload (the resource
	// depletion path). That overload intentionally skips defeat checking, so
	// the player must remain ALIVE even though no buildings would remain by
	// that count.
	auto other = std::make_shared<GameEntity>(99);
	auto ownership = std::make_shared<component::Ownership>(loop);
	ownership->set_owner(t0, 0);
	other->add_component(ownership);
	state->add_game_entity(other);

	state->remove_game_entity(99); // no defeat check
	TESTEQUALS(p0->get_state() == player_state_t::ALIVE, true);

	// Destroying the last building via the (id, time) overload defeats the player.
	state->remove_game_entity(10, t0);
	TESTEQUALS(p0->get_state() == player_state_t::DEFEATED, true);
}

void stance_component() {
	auto loop = std::make_shared<event::EventLoop>();
	auto t0 = time::time_t::from_int(0);
	auto t1 = time::time_t::from_int(1);

	component::Stance stance{loop};
	TESTEQUALS(stance.get_stance(t0) == component::stance_t::AGGRESSIVE, true);

	stance.set_stance(t1, component::stance_t::DEFENSIVE);
	TESTEQUALS(stance.get_stance(t1) == component::stance_t::DEFENSIVE, true);
	TESTEQUALS(stance.get_stance(t0) == component::stance_t::AGGRESSIVE, true);

	stance.set_stance(t1, component::stance_t::NO_ATTACK);
	TESTEQUALS(stance.get_stance(t1) == component::stance_t::NO_ATTACK, true);

	stance.set_stance(t1, component::stance_t::STAND_GROUND);
	TESTEQUALS(stance.get_stance(t1) == component::stance_t::STAND_GROUND, true);
}

void next_command_conditions_extended() {
	auto loop = std::make_shared<event::EventLoop>();
	auto entity = make_entity_with_command_queue(loop, 0);
	auto command_queue = std::dynamic_pointer_cast<component::CommandQueue>(
		entity->get_component(component::component_t::COMMANDQUEUE));
	auto t0 = time::time_t::from_int(0);

	// ATTACK_MOVE
	command_queue->add_command(t0,
	    std::make_shared<component::command::AttackMoveCommand>(coord::phys3{1, 1, 0}));
	TESTEQUALS(activity::next_command_attack_move(t0, entity), true);
	TESTEQUALS(activity::next_command_patrol(t0, entity), false);
	TESTEQUALS(activity::next_command_guard(t0, entity), false);
	command_queue->pop_command(t0);

	// PATROL
	command_queue->add_command(t0,
	    std::make_shared<component::command::PatrolCommand>(
	        coord::phys3{0, 0, 0}, coord::phys3{5, 5, 0}));
	TESTEQUALS(activity::next_command_attack_move(t0, entity), false);
	TESTEQUALS(activity::next_command_patrol(t0, entity), true);
	TESTEQUALS(activity::next_command_guard(t0, entity), false);
	command_queue->pop_command(t0);

	// GUARD
	command_queue->add_command(t0,
	    std::make_shared<component::command::GuardCommand>(entity_id_t{99}));
	TESTEQUALS(activity::next_command_attack_move(t0, entity), false);
	TESTEQUALS(activity::next_command_patrol(t0, entity), false);
	TESTEQUALS(activity::next_command_guard(t0, entity), true);
	command_queue->pop_command(t0);
}

} // namespace openage::gamestate::tests
