// Copyright 2025-2026 the openage authors. See copying.md for legal info.

#include "../testing/testing.h"

#include <memory>
#include <string>
#include <vector>

#include <nyan/nyan.h>

#include "coord/phys.h"
#include "coord/tile.h"
#include "event/eventhandler.h"
#include "event/event_loop.h"
#include "activity/condition/next_command.h"
#include "component/api/live.h"
#include "component/internal/command_queue.h"
#include "component/internal/commands/attack.h"
#include "component/internal/commands/attack_move.h"
#include "component/internal/commands/build.h"
#include "component/internal/commands/deconstruct.h"
#include "component/internal/commands/formation_move.h"
#include "component/internal/commands/garrison.h"
#include "component/internal/commands/gather.h"
#include "component/internal/commands/guard.h"
#include "component/internal/commands/idle.h"
#include "component/internal/commands/move.h"
#include "component/internal/commands/patrol.h"
#include "component/internal/commands/repair.h"
#include "component/internal/commands/train.h"
#include "component/internal/ownership.h"
#include "component/internal/position.h"
#include "system/garrison.h"
#include "system/repair.h"
#include "system/trade.h"
#include "component/internal/commands/trade.h"
#include "component/internal/salvage.h"
#include "component/internal/stance.h"
#include "definitions.h"
#include "event/deconstruct_complete.h"
#include "event/game_over.h"
#include "event/send_command.h"
#include "api/creatable.h"
#include "api/population.h"
#include "api/building_kind.h"
#include "fog_of_war.h"
#include "game_entity.h"
#include "game_state.h"
#include "map.h"
#include "pathfinding/types.h"
#include "player.h"
#include "terrain.h"
#include "terrain_chunk.h"
#include "renderer/stages/world/render_entity.h"
#include "time/time.h"


namespace openage::gamestate::tests {


namespace {

std::shared_ptr<GameEntity> make_entity_with_command_queue(const std::shared_ptr<openage::event::EventLoop> &loop,
                                                           entity_id_t id) {
	auto entity = std::make_shared<GameEntity>(id);
	entity->add_component(std::make_shared<component::CommandQueue>(loop));

	return entity;
}

} // namespace

void player_resources() {
	auto loop = std::make_shared<openage::event::EventLoop>();
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

void player_population() {
	auto loop = std::make_shared<openage::event::EventLoop>();
	auto db = nyan::Database::create();
	auto view = db->new_view();
	Player player{0, view, loop};

	auto t0 = time::time_t::from_int(0);
	auto t1 = time::time_t::from_int(1);
	auto t2 = time::time_t::from_int(2);

	// Fresh player: no demand, no capacity, no space — cannot support a unit.
	TESTEQUALS(player.get_population_demand(t0), 0);
	TESTEQUALS(player.get_population_capacity(t0), 0);
	TESTEQUALS(player.get_population_space(t0), 0);
	TESTEQUALS(player.has_population_space(t0, 1), false);

	// Give the player headroom for 5 population.
	player.init_population(t0, 5);
	TESTEQUALS(player.get_population_capacity(t0), 5);
	TESTEQUALS(player.get_population_space(t0), 5);
	TESTEQUALS(player.has_population_space(t0, 5), true);
	TESTEQUALS(player.has_population_space(t0, 6), false);

	// Reserve 4 population (e.g. four villagers queued).
	player.add_population_demand(t1, 4);
	TESTEQUALS(player.get_population_demand(t1), 4);
	TESTEQUALS(player.get_population_space(t1), 1);
	TESTEQUALS(player.has_population_space(t1, 1), true);
	TESTEQUALS(player.has_population_space(t1, 2), false);

	// A house raises capacity; now there is room again.
	player.add_population_capacity(t1, 5);
	TESTEQUALS(player.get_population_capacity(t1), 10);
	TESTEQUALS(player.get_population_space(t1), 6);

	// A unit dies: demand drops, space recovers. Past values are unchanged.
	player.add_population_demand(t2, -1);
	TESTEQUALS(player.get_population_demand(t2), 3);
	TESTEQUALS(player.get_population_demand(t1), 4);

	// Demand never records below 0 even if over-released.
	player.add_population_demand(t2, -100);
	TESTEQUALS(player.get_population_demand(t2), 0);

	// Capacity is clamped to POPULATION_MAX.
	player.init_population(t2, POPULATION_MAX + 50);
	TESTEQUALS(player.get_population_capacity(t2), POPULATION_MAX);
}

void next_command_conditions() {
	auto loop = std::make_shared<openage::event::EventLoop>();
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
	auto loop = std::make_shared<openage::event::EventLoop>();
	auto db = nyan::Database::create();
	auto state = std::make_shared<GameState>(db, loop);
	auto entity = make_entity_with_command_queue(loop, 7);
	auto t0 = time::time_t::from_int(0);

	state->add_game_entity(entity);

	gamestate::event::SendCommandHandler handler;

	handler.invoke(*loop,
	               nullptr,
	               state,
	               t0,
	               openage::event::EventHandler::param_map{
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
	               openage::event::EventHandler::param_map{
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
	               openage::event::EventHandler::param_map{
	                   {"type", component::command::command_t::TRAIN},
	                   {"game_entity", std::string{"test.unit.Spearman"}},
	                   {"entity_ids", std::vector<entity_id_t>{7}},
	               });

	auto train_command = std::dynamic_pointer_cast<component::command::TrainCommand>(
		command_queue->pop_command(t0));
	TESTEQUALS(train_command != nullptr, true);
	TESTEQUALS(train_command->get_game_entity(), std::string{"test.unit.Spearman"});

	handler.invoke(*loop,
	               nullptr,
	               state,
	               t0,
	               openage::event::EventHandler::param_map{
	                   {"type", component::command::command_t::DECONSTRUCT},
	                   {"target_entity_id", entity_id_t{88}},
	                   {"entity_ids", std::vector<entity_id_t>{7}},
	               });

	auto deconstruct_command = std::dynamic_pointer_cast<component::command::DeconstructCommand>(
		command_queue->pop_command(t0));
	TESTEQUALS(deconstruct_command != nullptr, true);
	TESTEQUALS(deconstruct_command->get_target(), 88);

	handler.invoke(*loop,
	               nullptr,
	               state,
	               t0,
	               openage::event::EventHandler::param_map{
	                   {"type", component::command::command_t::REPAIR},
	                   {"target_entity_id", entity_id_t{99}},
	                   {"entity_ids", std::vector<entity_id_t>{7}},
	               });

	auto repair_command = std::dynamic_pointer_cast<component::command::RepairCommand>(
		command_queue->pop_command(t0));
	TESTEQUALS(repair_command != nullptr, true);
	TESTEQUALS(repair_command->get_target(), 99);

	handler.invoke(*loop,
	               nullptr,
	               state,
	               t0,
	               openage::event::EventHandler::param_map{
	                   {"type", component::command::command_t::GARRISON},
	                   {"target_entity_id", entity_id_t{105}},
	                   {"entity_ids", std::vector<entity_id_t>{7}},
	               });

	auto garrison_command = std::dynamic_pointer_cast<component::command::GarrisonCommand>(
		command_queue->pop_command(t0));
	TESTEQUALS(garrison_command != nullptr, true);
	TESTEQUALS(garrison_command->get_target(), 105);

	handler.invoke(*loop,
	               nullptr,
	               state,
	               t0,
	               openage::event::EventHandler::param_map{
	                   {"type", component::command::command_t::UNGARRISON},
	                   {"entity_ids", std::vector<entity_id_t>{7}},
	               });

	auto ungarrison_command = std::dynamic_pointer_cast<component::command::UngarrisonCommand>(
		command_queue->pop_command(t0));
	TESTEQUALS(ungarrison_command != nullptr, true);
}

void production_requests() {
	auto loop = std::make_shared<openage::event::EventLoop>();
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
	auto loop = std::make_shared<openage::event::EventLoop>();
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

void rally_point_lifecycle() {
	auto loop = std::make_shared<openage::event::EventLoop>();
	auto db = nyan::Database::create();
	auto state = std::make_shared<GameState>(db, loop);

	auto t0 = time::time_t::from_int(0);

	// A producing building exists in the state.
	auto building = std::make_shared<GameEntity>(7);
	state->add_game_entity(building);

	TESTEQUALS(state->has_rally_point(7), false);
	TESTEQUALS(state->get_rally_point(7).has_value(), false);

	coord::phys3 rally{20, 30, 0};
	state->set_rally_point(7, rally);
	TESTEQUALS(state->has_rally_point(7), true);
	auto stored = state->get_rally_point(7);
	TESTEQUALS(stored.has_value(), true);
	TESTEQUALS(stored.value() == rally, true);

	// Setting again overwrites the previous point.
	coord::phys3 rally2{5, 6, 0};
	state->set_rally_point(7, rally2);
	TESTEQUALS(state->get_rally_point(7).value() == rally2, true);

	// Explicit clear removes it.
	state->clear_rally_point(7);
	TESTEQUALS(state->has_rally_point(7), false);

	// The rally point is cleaned up when the producing entity is destroyed.
	state->set_rally_point(7, rally);
	TESTEQUALS(state->has_rally_point(7), true);
	state->remove_game_entity(7, t0);
	TESTEQUALS(state->has_rally_point(7), false);
}

void player_state_transitions() {
	auto loop = std::make_shared<openage::event::EventLoop>();
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
              const std::shared_ptr<openage::event::EventLoop> &loop,
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
	auto loop = std::make_shared<openage::event::EventLoop>();
	auto db = nyan::Database::create();
	auto state = std::make_shared<GameState>(db, loop);

	// check_defeat fires events via the loop; register their handlers.
	loop->add_event_handler(std::make_shared<gamestate::event::PlayerDefeatedHandler>());
	loop->add_event_handler(std::make_shared<gamestate::event::GameOverHandler>());

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
	TESTEQUALS(state->get_game_result().finished, true);
	TESTEQUALS(state->get_game_result().has_winner, true);
	TESTEQUALS(state->get_game_result().winner_id, player_id_t{1});

	state->clear_game_result();
	TESTEQUALS(state->get_game_result().finished, false);
	TESTEQUALS(state->get_game_result().has_winner, false);
}

void building_population_capacity() {
	auto loop = std::make_shared<openage::event::EventLoop>();
	auto db = nyan::Database::create();
	auto state = std::make_shared<GameState>(db, loop);

	// Destroying a building runs check_defeat, which fires events via the loop.
	loop->add_event_handler(std::make_shared<gamestate::event::PlayerDefeatedHandler>());
	loop->add_event_handler(std::make_shared<gamestate::event::GameOverHandler>());

	auto view = db->new_view();
	auto p0 = std::make_shared<Player>(0, view, loop);
	state->add_player(p0);

	auto t0 = time::time_t::from_int(0);

	// Two completed buildings' worth of population headroom, each with a
	// recorded provision matching the capacity they contributed at spawn.
	p0->init_population(t0, 0);
	p0->add_population_capacity(t0, 2 * DEFAULT_BUILDING_POPULATION_SPACE);
	TESTEQUALS(p0->get_population_capacity(t0), 2 * DEFAULT_BUILDING_POPULATION_SPACE);

	make_building(10, 0, loop, state, t0);
	make_building(11, 0, loop, state, t0);
	state->set_entity_population_provision(10, DEFAULT_BUILDING_POPULATION_SPACE);
	state->set_entity_population_provision(11, DEFAULT_BUILDING_POPULATION_SPACE);

	// Destroying one building releases the headroom it provided; the player is
	// still alive (one building remains), so demand/capacity bookkeeping — not
	// defeat — is what changes.
	state->remove_game_entity(10, t0);
	TESTEQUALS(p0->get_state() == player_state_t::ALIVE, true);
	TESTEQUALS(p0->get_population_capacity(t0), DEFAULT_BUILDING_POPULATION_SPACE);
}

void no_defeat_for_unit_death() {
	auto loop = std::make_shared<openage::event::EventLoop>();
	auto db = nyan::Database::create();
	auto state = std::make_shared<GameState>(db, loop);

	// check_defeat fires events via the loop; register their handlers.
	loop->add_event_handler(std::make_shared<gamestate::event::PlayerDefeatedHandler>());
	loop->add_event_handler(std::make_shared<gamestate::event::GameOverHandler>());

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
	auto loop = std::make_shared<openage::event::EventLoop>();
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
	auto loop = std::make_shared<openage::event::EventLoop>();
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

	// DECONSTRUCT
	command_queue->add_command(t0,
	                           std::make_shared<component::command::DeconstructCommand>(entity_id_t{55}));
	TESTEQUALS(activity::next_command_deconstruct(t0, entity), true);
	TESTEQUALS(activity::next_command_build(t0, entity), false);
	command_queue->pop_command(t0);

	// REPAIR
	command_queue->add_command(t0,
	                           std::make_shared<component::command::RepairCommand>(entity_id_t{66}));
	TESTEQUALS(activity::next_command_repair(t0, entity), true);
	TESTEQUALS(activity::next_command_build(t0, entity), false);
	command_queue->pop_command(t0);

	// GARRISON
	command_queue->add_command(t0,
	                           std::make_shared<component::command::GarrisonCommand>(entity_id_t{77}));
	TESTEQUALS(activity::next_command_garrison(t0, entity), true);
	TESTEQUALS(activity::next_command_build(t0, entity), false);
	command_queue->pop_command(t0);

	// UNGARRISON
	command_queue->add_command(t0,
	                           std::make_shared<component::command::UngarrisonCommand>());
	TESTEQUALS(activity::next_command_ungarrison(t0, entity), true);
	TESTEQUALS(activity::next_command_build(t0, entity), false);
	command_queue->pop_command(t0);
}

void next_command_formation_move_test() {
	auto loop = std::make_shared<openage::event::EventLoop>();
	auto entity = make_entity_with_command_queue(loop, 0);
	auto command_queue = std::dynamic_pointer_cast<component::CommandQueue>(
		entity->get_component(component::component_t::COMMANDQUEUE));
	auto t0 = time::time_t::from_int(0);

	// Queue a FormationMoveCommand and verify the condition detects it.
	command_queue->add_command(
		t0,
		std::make_shared<component::command::FormationMoveCommand>(
			coord::phys3{10, 10, 0},
			coord::phys3_delta{1, 0, 0}));
	TESTEQUALS(activity::next_command_formation_move(t0, entity), true);
	TESTEQUALS(activity::next_command_move(t0, entity), false);
	TESTEQUALS(activity::next_command_idle(t0, entity), false);
	command_queue->pop_command(t0);

	// Queue a regular MoveCommand and verify formation_move returns false.
	command_queue->add_command(
		t0,
		std::make_shared<component::command::MoveCommand>(coord::phys3{5, 5, 0}));
	TESTEQUALS(activity::next_command_formation_move(t0, entity), false);
	TESTEQUALS(activity::next_command_move(t0, entity), true);
	command_queue->pop_command(t0);
}

void fog_of_war_exploration() {
	FogOfWar fow;
	player_id_t p0{0};
	coord::tile center{5, 5};

	// Initially nothing explored.
	TESTEQUALS(fow.is_explored(p0, center), false);
	TESTEQUALS(fow.is_visible(p0, center), false);

	// update_visibility with range=1 should reveal a 3x3 area.
	fow.update_visibility(p0, center, 1);
	TESTEQUALS(fow.is_visible(p0, center), true);
	TESTEQUALS(fow.is_explored(p0, center), true);

	// Adjacent tile within range should be visible and explored.
	coord::tile adj{6, 5};
	TESTEQUALS(fow.is_visible(p0, adj), true);
	TESTEQUALS(fow.is_explored(p0, adj), true);

	// Tile outside range-1 from center should not be visible.
	coord::tile far{8, 8};
	TESTEQUALS(fow.is_visible(p0, far), false);
	TESTEQUALS(fow.is_explored(p0, far), false);
}

void fog_of_war_visibility() {
	FogOfWar fow;
	player_id_t p0{0};
	coord::tile center{5, 5};

	fow.update_visibility(p0, center, 1);
	TESTEQUALS(fow.is_visible(p0, center), true);

	// flush_visible should clear current visibility but not explored status.
	fow.flush_visible(p0);
	TESTEQUALS(fow.is_visible(p0, center), false);
	TESTEQUALS(fow.is_explored(p0, center), true);

	// Different player should not share visibility.
	player_id_t p1{1};
	TESTEQUALS(fow.is_visible(p1, center), false);
	TESTEQUALS(fow.is_explored(p1, center), false);

	// Update p1's visibility at a different location.
	coord::tile other{20, 20};
	fow.update_visibility(p1, other, 0);
	TESTEQUALS(fow.is_visible(p1, other), true);
	TESTEQUALS(fow.is_visible(p0, other), false);
}

void tile_occupancy() {
	auto loop = std::make_shared<openage::event::EventLoop>();
	auto db = std::make_shared<nyan::Database>();
	auto state = std::make_shared<GameState>(db, loop);

	coord::tile t0{5, 5};
	coord::tile t1{6, 6};
	entity_id_t e0{100};
	entity_id_t e1{200};

	// Initially no tile is occupied.
	TESTEQUALS(state->is_tile_occupied(t0), false);
	TESTEQUALS(state->get_tile_occupant(t0).has_value(), false);

	// Occupy a tile.
	state->occupy_tile(t0, e0);
	TESTEQUALS(state->is_tile_occupied(t0), true);
	TESTEQUALS(state->get_tile_occupant(t0).value(), e0);

	// Other tile still free.
	TESTEQUALS(state->is_tile_occupied(t1), false);

	// Moving the entity to a new tile releases the old one.
	state->occupy_tile(t1, e0);
	TESTEQUALS(state->is_tile_occupied(t0), false);
	TESTEQUALS(state->is_tile_occupied(t1), true);
	TESTEQUALS(state->get_tile_occupant(t1).value(), e0);

	// A second entity can occupy the released tile.
	state->occupy_tile(t0, e1);
	TESTEQUALS(state->is_tile_occupied(t0), true);
	TESTEQUALS(state->get_tile_occupant(t0).value(), e1);

	// Release explicitly.
	state->release_tile(e1);
	TESTEQUALS(state->is_tile_occupied(t0), false);

	// Release a non-existent entity is safe.
	state->release_tile(entity_id_t{999});
}

void last_known_positions() {
	FogOfWar fow;
	player_id_t p0{0};
	entity_id_t e0{10};
	entity_id_t e1{20};

	// Initially no last-known position.
	TESTEQUALS(fow.get_last_known_position(p0, e0).has_value(), false);

	// Record a position.
	coord::phys3 pos{3, 4, 0};
	fow.set_last_known_position(p0, e0, pos);
	auto result = fow.get_last_known_position(p0, e0);
	TESTEQUALS(result.has_value(), true);
	TESTEQUALS(result.value().ne, pos.ne);
	TESTEQUALS(result.value().se, pos.se);

	// Different entity not affected.
	TESTEQUALS(fow.get_last_known_position(p0, e1).has_value(), false);

	// Different player not affected.
	player_id_t p1{1};
	TESTEQUALS(fow.get_last_known_position(p1, e0).has_value(), false);

	// Clear the entry.
	fow.clear_last_known_position(p0, e0);
	TESTEQUALS(fow.get_last_known_position(p0, e0).has_value(), false);

	// Clearing a non-existent entry is safe.
	fow.clear_last_known_position(p0, entity_id_t{999});
}

void fog_of_war_refresh() {
	auto loop = std::make_shared<openage::event::EventLoop>();
	auto db = std::make_shared<nyan::Database>();
	auto state = std::make_shared<GameState>(db, loop);
	auto t0 = time::time_t::from_int(0);

	auto player = std::make_shared<Player>(player_id_t{0}, db->new_view(), loop);
	state->add_player(player);

	auto make_scout = [&](entity_id_t id, coord::phys3 pos) {
		auto entity = std::make_shared<GameEntity>(id);
		auto position = std::make_shared<component::Position>(loop);
		position->set_position(t0, pos);
		entity->add_component(position);

		auto ownership = std::make_shared<component::Ownership>(loop);
		ownership->set_owner(t0, player_id_t{0});
		entity->add_component(ownership);

		state->add_game_entity(entity);
	};

	make_scout(entity_id_t{1}, coord::phys3{10, 10, 0});
	make_scout(entity_id_t{2}, coord::phys3{50, 50, 0});

	state->refresh_visibility(t0);

	coord::tile near_scout{10, 10};
	coord::tile far_from_scouts{0, 0};
	TESTEQUALS(state->is_tile_visible(player_id_t{0}, near_scout), true);
	TESTEQUALS(state->is_tile_explored(player_id_t{0}, near_scout), true);
	TESTEQUALS(state->is_tile_visible(player_id_t{0}, far_from_scouts), false);

	state->refresh_visibility(t0);
	TESTEQUALS(state->is_tile_visible(player_id_t{0}, far_from_scouts), false);
}

void entity_visibility_query() {
	auto loop = std::make_shared<openage::event::EventLoop>();
	auto db = std::make_shared<nyan::Database>();
	auto state = std::make_shared<GameState>(db, loop);
	auto t0 = time::time_t::from_int(0);

	auto observer = std::make_shared<Player>(player_id_t{0}, db->new_view(), loop);
	auto enemy_player = std::make_shared<Player>(player_id_t{1}, db->new_view(), loop);
	state->add_player(observer);
	state->add_player(enemy_player);

	auto make_unit = [&](entity_id_t id, player_id_t owner, coord::phys3 pos) {
		auto entity = std::make_shared<GameEntity>(id);
		auto position = std::make_shared<component::Position>(loop);
		position->set_position(t0, pos);
		entity->add_component(position);

		auto ownership = std::make_shared<component::Ownership>(loop);
		ownership->set_owner(t0, owner);
		entity->add_component(ownership);

		state->add_game_entity(entity);
	};

	make_unit(entity_id_t{1}, player_id_t{0}, coord::phys3{10, 10, 0});
	make_unit(entity_id_t{2}, player_id_t{1}, coord::phys3{12, 12, 0});

	state->refresh_visibility(t0);
	TESTEQUALS(state->is_entity_visible(player_id_t{0}, entity_id_t{2}, t0), true);

	auto enemy_entity = state->get_game_entity(entity_id_t{2});
	auto enemy_pos = std::dynamic_pointer_cast<component::Position>(
		enemy_entity->get_component(component::component_t::POSITION));
	enemy_pos->set_position(t0, coord::phys3{100, 100, 0});

	state->refresh_visibility(t0);
	TESTEQUALS(state->is_entity_visible(player_id_t{0}, entity_id_t{2}, t0), false);
}

void fog_tile_texture() {
	auto loop = std::make_shared<openage::event::EventLoop>();
	auto db = std::make_shared<nyan::Database>();
	auto state = std::make_shared<GameState>(db, loop);
	auto t0 = time::time_t::from_int(0);

	auto observer = std::make_shared<Player>(player_id_t{0}, db->new_view(), loop);
	state->add_player(observer);

	auto make_scout = [&](entity_id_t id, coord::phys3 pos) {
		auto entity = std::make_shared<GameEntity>(id);
		auto position = std::make_shared<component::Position>(loop);
		position->set_position(t0, pos);
		entity->add_component(position);

		auto ownership = std::make_shared<component::Ownership>(loop);
		ownership->set_owner(t0, player_id_t{0});
		entity->add_component(ownership);

		state->add_game_entity(entity);
	};

	// Without a map the fog texture stays empty.
	state->refresh_visibility(t0);
	auto empty_fog = state->get_fog_tile_texture();
	TESTEQUALS(empty_fog.empty(), true);

	// Build a minimal 4x4 terrain and map.
	std::vector<std::shared_ptr<TerrainChunk>> chunks;
	auto chunk = std::make_shared<TerrainChunk>(
		util::Vector2s{4, 4},
		coord::tile_delta{0, 0},
		std::vector<TerrainTile>{});
	chunks.push_back(chunk);
	auto terrain = std::make_shared<Terrain>(util::Vector2s{4, 4}, std::move(chunks));
	state->set_map(std::make_shared<Map>(state, terrain));

	make_scout(entity_id_t{1}, coord::phys3{2, 2, 0});

	state->refresh_visibility(t0);
	auto fog = state->get_fog_tile_texture();
	TESTEQUALS(fog.empty(), false);
	TESTEQUALS(fog.size[0], size_t{4});
	TESTEQUALS(fog.size[1], size_t{4});
	TESTEQUALS(fog.pixels.size(), size_t{4 * 4 * 4});

	// Scout at tile (2,2) on a 4x4 map should be visible (default sight range 4).
	TESTEQUALS(fog.pixels[(2 + 2 * 4) * 4] > 200, true);
}

void minimap_texture() {
	auto loop = std::make_shared<openage::event::EventLoop>();
	auto db = std::make_shared<nyan::Database>();
	auto state = std::make_shared<GameState>(db, loop);
	auto t0 = time::time_t::from_int(0);

	auto observer = std::make_shared<Player>(player_id_t{0}, db->new_view(), loop);
	state->add_player(observer);

	std::vector<std::shared_ptr<TerrainChunk>> chunks;
	auto chunk = std::make_shared<TerrainChunk>(
		util::Vector2s{4, 4},
		coord::tile_delta{0, 0},
		std::vector<TerrainTile>{});
	chunks.push_back(chunk);
	auto terrain = std::make_shared<Terrain>(util::Vector2s{4, 4}, std::move(chunks));
	state->set_map(std::make_shared<Map>(state, terrain));

	auto unit = std::make_shared<GameEntity>(entity_id_t{1});
	auto position = std::make_shared<component::Position>(loop);
	position->set_position(t0, coord::phys3{2, 2, 0});
	unit->add_component(position);
	auto ownership = std::make_shared<component::Ownership>(loop);
	ownership->set_owner(t0, player_id_t{0});
	unit->add_component(ownership);
	state->add_game_entity(unit);

	state->refresh_visibility(t0);
	auto minimap = state->get_minimap_texture();
	TESTEQUALS(minimap.empty(), false);
	TESTEQUALS(minimap.size[0], size_t{4});
	TESTEQUALS(minimap.pixels.size(), size_t{4 * 4 * 4});

	// Unit marker at tile (2,2) tints the minimap blue (own unit).
	const size_t idx = (2 + 2 * 4) * 4;
	TESTEQUALS(minimap.pixels[idx + 2] > 200, true);
}

void hazard_path_costs_no_map() {
	auto loop = std::make_shared<openage::event::EventLoop>();
	auto db = std::make_shared<nyan::Database>();
	auto state = std::make_shared<GameState>(db, loop);
	auto t0 = time::time_t::from_int(0);

	// Safe no-op when no map or grids exist yet.
	state->apply_hazard_path_costs(player_id_t{0}, path::grid_id_t{0}, t0);
}

void fog_render_visibility() {
	auto loop = std::make_shared<openage::event::EventLoop>();
	auto db = std::make_shared<nyan::Database>();
	auto state = std::make_shared<GameState>(db, loop);
	auto t0 = time::time_t::from_int(0);

	auto observer = std::make_shared<Player>(player_id_t{0}, db->new_view(), loop);
	auto enemy_player = std::make_shared<Player>(player_id_t{1}, db->new_view(), loop);
	state->add_player(observer);
	state->add_player(enemy_player);

	auto attach_render = [](const std::shared_ptr<GameEntity> &entity) {
		entity->set_render_entity(std::make_shared<renderer::world::RenderEntity>());
	};

	auto make_unit = [&](entity_id_t id, player_id_t owner, coord::phys3 pos) {
		auto entity = std::make_shared<GameEntity>(id);
		auto position = std::make_shared<component::Position>(loop);
		position->set_position(t0, pos);
		entity->add_component(position);

		auto ownership = std::make_shared<component::Ownership>(loop);
		ownership->set_owner(t0, owner);
		entity->add_component(ownership);

		attach_render(entity);
		state->add_game_entity(entity);
		return entity;
	};

	make_unit(entity_id_t{1}, player_id_t{0}, coord::phys3{10, 10, 0});
	auto enemy = make_unit(entity_id_t{2}, player_id_t{1}, coord::phys3{12, 12, 0});
	auto enemy_render = enemy->get_render_entity();

	state->refresh_visibility(t0);
	TESTEQUALS(enemy_render->get_fog_display() == renderer::world::fog_display_t::VISIBLE, true);

	auto enemy_pos = std::dynamic_pointer_cast<component::Position>(
		enemy->get_component(component::component_t::POSITION));
	enemy_pos->set_position(t0, coord::phys3{100, 100, 0});

	state->refresh_visibility(t0);
	TESTEQUALS(enemy_render->get_fog_display() == renderer::world::fog_display_t::GHOST, true);
	TESTEQUALS(enemy_render->get_ghost_position().has_value(), true);

	// Own units stay visible regardless of fog tiles.
	auto own_render = state->get_game_entity(entity_id_t{1})->get_render_entity();
	TESTEQUALS(own_render->get_fog_display() == renderer::world::fog_display_t::VISIBLE, true);
}

void next_command_build_test() {
	auto loop = std::make_shared<openage::event::EventLoop>();
	auto entity = make_entity_with_command_queue(loop, 0);
	auto command_queue = std::dynamic_pointer_cast<component::CommandQueue>(
		entity->get_component(component::component_t::COMMANDQUEUE));
	auto t0 = time::time_t::from_int(0);

	// Queue a BuildCommand and verify the condition detects it.
	command_queue->add_command(
		t0,
		std::make_shared<component::command::BuildCommand>(
			"test.building.TownCenter",
			coord::phys3{10, 10, 0}));
	TESTEQUALS(activity::next_command_build(t0, entity), true);
	TESTEQUALS(activity::next_command_train(t0, entity), false);
	TESTEQUALS(activity::next_command_move(t0, entity), false);
	command_queue->pop_command(t0);

	// After popping, condition returns false.
	TESTEQUALS(activity::next_command_build(t0, entity), false);

	// Queue a TrainCommand and verify build returns false.
	command_queue->add_command(
		t0,
		std::make_shared<component::command::TrainCommand>("test.unit.Villager"));
	TESTEQUALS(activity::next_command_build(t0, entity), false);
	TESTEQUALS(activity::next_command_train(t0, entity), true);
	command_queue->pop_command(t0);
}

// A built building must be placed at the position the player selected, not at
// the builder's location (the way TRAIN spawns units). The Build system carries
// that position to the spawn handler through the event param_map under the
// "spawn_pos" key; this test guards both the command payload and the param_map
// round-trip that the handler relies on to choose explicit placement.
void build_command_placement() {
	auto loop = std::make_shared<openage::event::EventLoop>();
	auto entity = make_entity_with_command_queue(loop, 0);
	auto command_queue = std::dynamic_pointer_cast<component::CommandQueue>(
		entity->get_component(component::component_t::COMMANDQUEUE));
	auto t0 = time::time_t::from_int(0);

	// The build command preserves both the building fqon and the target site.
	auto build_site = coord::phys3{42, 17, 0};
	command_queue->add_command(
		t0,
		std::make_shared<component::command::BuildCommand>(
			"test.building.TownCenter",
			build_site));
	auto build_command = std::dynamic_pointer_cast<component::command::BuildCommand>(
		command_queue->pop_command(t0));
	TESTEQUALS(build_command != nullptr, true);
	TESTEQUALS(build_command->get_building(), std::string{"test.building.TownCenter"});
	TESTEQUALS(build_command->get_target() == build_site, true);

	// BUILD events carry the site under "spawn_pos"; the handler detects this and
	// uses it verbatim instead of deriving the position from the producer.
	openage::event::EventHandler::param_map build_params{
		{"owner", player_id_t{0}},
		{"game_entity", build_command->get_building()},
		{"spawn_pos", build_command->get_target()},
	};
	TESTEQUALS(build_params.check_type<coord::phys3>("spawn_pos"), true);
	TESTEQUALS(build_params.get("spawn_pos", WORLD_ORIGIN) == build_site, true);

	// TRAIN events omit "spawn_pos"; the handler must fall back to its
	// producer-derived position, so the explicit-position check is false.
	openage::event::EventHandler::param_map train_params{
		{"owner", player_id_t{0}},
		{"game_entity", std::string{"test.unit.Villager"}},
	};
	TESTEQUALS(train_params.check_type<coord::phys3>("spawn_pos"), false);
}

void building_cost_and_salvage_spawn() {
	auto loop = std::make_shared<openage::event::EventLoop>();
	auto db = nyan::Database::create();
	auto state = std::make_shared<GameState>(db, loop);
	auto t0 = time::time_t::from_int(0);

	BuildingCostRecord cost;
	cost.entries.push_back(ResourceCostEntry{"test.resource.Wood", 200});
	cost.destroy_recovery_fraction = 0.5;
	state->set_building_cost(10, cost);

	auto building = std::make_shared<GameEntity>(10);
	building->add_component(std::make_shared<component::Ownership>(loop));
	building->add_component(std::make_shared<component::Position>(loop));
	auto pos = std::dynamic_pointer_cast<component::Position>(
		building->get_component(component::component_t::POSITION));
	pos->set_position(t0, coord::phys3{5, 5, 0});
	state->add_game_entity(building);

	auto cost_after = state->get_building_cost(10);
	TESTEQUALS(cost_after.has_value(), true);
	TESTEQUALS(cost_after->entries.size(), 1);
	TESTEQUALS(cost_after->entries[0].amount, 200);

	loop->add_event_handler(std::make_shared<gamestate::event::PlayerDefeatedHandler>());
	loop->add_event_handler(std::make_shared<gamestate::event::GameOverHandler>());

	state->remove_game_entity(10, t0);

	TESTEQUALS(state->get_building_cost(10).has_value(), false);
	TESTEQUALS(state->get_game_entities().size(), 1);

	const auto &salvage_entity = state->get_game_entities().begin()->second;
	TESTEQUALS(salvage_entity->has_component(component::component_t::SALVAGE), true);
	auto salvage = std::dynamic_pointer_cast<component::Salvage>(
		salvage_entity->get_component(component::component_t::SALVAGE));
	TESTEQUALS(salvage->get_resource_type(), std::string{"test.resource.Wood"});
	TESTEQUALS(salvage->get_amount(t0), 100);
}

void building_cost_fraction_clamped() {
	BuildingCostRecord cost;
	cost.entries.push_back(ResourceCostEntry{"test.resource.Food", 100});
	cost.destroy_recovery_fraction = 2.5;
	cost.deconstruct_recovery_fraction = -0.2;

	auto normalized = api::normalize_building_cost(cost);
	TESTEQUALS(normalized.destroy_recovery_fraction, 1.0);
	TESTEQUALS(normalized.deconstruct_recovery_fraction, 0.0);
}

void deconstruct_complete_spawns_salvage_if_building_gone() {
	auto loop = std::make_shared<openage::event::EventLoop>();
	auto db = nyan::Database::create();
	auto state = std::make_shared<GameState>(db, loop);
	auto t0 = time::time_t::from_int(0);

	gamestate::event::DeconstructCompleteHandler handler;
	handler.invoke(*loop,
	               nullptr,
	               state,
	               t0,
	               openage::event::EventHandler::param_map{
	                   {"building_id", entity_id_t{99}},
	                   {"building_pos", coord::phys3{3, 4, 0}},
	                   {"build_cost",
	                    BuildingCostRecord{
	                        {ResourceCostEntry{"test.resource.Wood", 80}},
	                        SALVAGE_RECOVERY_FRACTION,
	                        0.75,
	                        0.0,
	                    }},
	                   {"recovery_fraction", 0.75},
	               });

	TESTEQUALS(state->get_game_entities().size(), 1);
	const auto &pile = state->get_game_entities().begin()->second;
	TESTEQUALS(pile->has_component(component::component_t::SALVAGE), true);
	auto salvage = std::dynamic_pointer_cast<component::Salvage>(
		pile->get_component(component::component_t::SALVAGE));
	TESTEQUALS(salvage->get_amount(t0), 60);
}

void building_cost_custom_salvage_fraction() {
	auto loop = std::make_shared<openage::event::EventLoop>();
	auto db = nyan::Database::create();
	auto state = std::make_shared<GameState>(db, loop);
	auto t0 = time::time_t::from_int(0);

	BuildingCostRecord cost;
	cost.entries.push_back(ResourceCostEntry{"test.resource.Stone", 100});
	cost.destroy_recovery_fraction = 0.3;
	state->set_building_cost(20, cost);

	auto building = std::make_shared<GameEntity>(20);
	building->add_component(std::make_shared<component::Ownership>(loop));
	building->add_component(std::make_shared<component::Position>(loop));
	auto pos = std::dynamic_pointer_cast<component::Position>(
		building->get_component(component::component_t::POSITION));
	pos->set_position(t0, coord::phys3{1, 1, 0});
	state->add_game_entity(building);

	loop->add_event_handler(std::make_shared<gamestate::event::PlayerDefeatedHandler>());
	loop->add_event_handler(std::make_shared<gamestate::event::GameOverHandler>());

	state->remove_game_entity(20, t0);

	const auto &salvage_entity = state->get_game_entities().begin()->second;
	auto salvage = std::dynamic_pointer_cast<component::Salvage>(
		salvage_entity->get_component(component::component_t::SALVAGE));
	TESTEQUALS(salvage->get_amount(t0), 30);
}

void building_cost_multi_resource_salvage() {
	auto loop = std::make_shared<openage::event::EventLoop>();
	auto db = nyan::Database::create();
	auto state = std::make_shared<GameState>(db, loop);
	auto t0 = time::time_t::from_int(0);

	BuildingCostRecord cost;
	cost.entries.push_back(ResourceCostEntry{"test.resource.Wood", 200});
	cost.entries.push_back(ResourceCostEntry{"test.resource.Stone", 100});
	cost.destroy_recovery_fraction = 0.5;
	state->set_building_cost(30, cost);

	auto building = std::make_shared<GameEntity>(30);
	building->add_component(std::make_shared<component::Ownership>(loop));
	building->add_component(std::make_shared<component::Position>(loop));
	auto pos = std::dynamic_pointer_cast<component::Position>(
		building->get_component(component::component_t::POSITION));
	pos->set_position(t0, coord::phys3{0, 0, 0});
	state->add_game_entity(building);

	loop->add_event_handler(std::make_shared<gamestate::event::PlayerDefeatedHandler>());
	loop->add_event_handler(std::make_shared<gamestate::event::GameOverHandler>());

	state->remove_game_entity(30, t0);

	TESTEQUALS(state->get_game_entities().size(), 2);

	int64_t wood = 0;
	int64_t stone = 0;
	for (const auto &[id, entity] : state->get_game_entities()) {
		(void) id;
		auto salvage = std::dynamic_pointer_cast<component::Salvage>(
			entity->get_component(component::component_t::SALVAGE));
		if (salvage->get_resource_type() == "test.resource.Wood") {
			wood = salvage->get_amount(t0);
		}
		else if (salvage->get_resource_type() == "test.resource.Stone") {
			stone = salvage->get_amount(t0);
		}
	}
	TESTEQUALS(wood, 100);
	TESTEQUALS(stone, 50);
}

void population_lookup_missing_entity() {
	auto db = nyan::Database::create();
	auto view = db->new_view();

	TESTEQUALS(api::lookup_population_demand(view, "test.unit.Missing"), 0);
	TESTEQUALS(api::lookup_population_provision(view, "test.building.Missing"), 0);
}

void entity_population_tracking() {
	auto loop = std::make_shared<openage::event::EventLoop>();
	auto db = nyan::Database::create();
	auto state = std::make_shared<GameState>(db, loop);

	// remove_game_entity may fire defeat events when the last building dies.
	loop->add_event_handler(std::make_shared<gamestate::event::PlayerDefeatedHandler>());
	loop->add_event_handler(std::make_shared<gamestate::event::GameOverHandler>());

	auto view = db->new_view();
	auto player = std::make_shared<Player>(0, view, loop);
	state->add_player(player);

	auto t0 = time::time_t::from_int(0);
	player->init_population(t0, 20);

	state->set_entity_population_demand(1, 3);
	TESTEQUALS(state->get_entity_population_demand(1).value_or(0), 3);

	// Building with a non-default recorded provision.
	auto building = std::make_shared<GameEntity>(2);
	auto building_owner = std::make_shared<component::Ownership>(loop);
	building_owner->set_owner(t0, 0);
	building->add_component(building_owner);
	state->set_entity_population_provision(2, 10);
	player->add_population_capacity(t0, 10);
	state->add_game_entity(building);

	state->remove_game_entity(2, t0);
	TESTEQUALS(player->get_population_capacity(t0), 20);
	TESTEQUALS(state->get_entity_population_provision(2).has_value(), false);
}

void salvage_decay() {
	auto loop = std::make_shared<openage::event::EventLoop>();
	auto db = nyan::Database::create();
	auto state = std::make_shared<GameState>(db, loop);
	auto t0 = time::time_t::from_int(0);
	auto t_decay = t0 + SALVAGE_DECAY_INTERVAL_SEC;

	auto entity = std::make_shared<GameEntity>(1);
	entity->add_component(std::make_shared<component::Position>(loop));
	auto salvage = std::make_shared<component::Salvage>(loop, "test.resource.Stone", 5, t0);
	entity->add_component(salvage);
	state->add_game_entity(entity);

	state->tick_salvage_decay(t0);
	TESTEQUALS(state->get_game_entities().size(), 1);
	TESTEQUALS(salvage->get_amount(t0), 5);

	state->tick_salvage_decay(t_decay);
	TESTEQUALS(state->get_game_entities().size(), 1);
	TESTEQUALS(salvage->get_amount(t_decay), 4);

	auto t_gone = t0 + SALVAGE_DECAY_INTERVAL_SEC * 6;
	state->tick_salvage_decay(t_gone);
	TESTEQUALS(state->get_game_entities().size(), 0);
}

void resource_node_regen() {
	auto loop = std::make_shared<openage::event::EventLoop>();
	auto db = nyan::Database::create();
	auto state = std::make_shared<GameState>(db, loop);
	auto t0 = time::time_t::from_int(0);

	// Regeneration is off by default (original game behaviour).
	TESTEQUALS(state->is_forest_regen_enabled(), false);

	state->set_forest_regen_enabled(true);
	TESTEQUALS(state->is_forest_regen_enabled(), true);

	// A harvestable resource node registered for regeneration.
	auto node = std::make_shared<GameEntity>(1);
	node->add_component(std::make_shared<component::Position>(loop));
	state->add_game_entity(node);

	state->register_resource_node(1, 100, t0);
	TESTEQUALS(state->is_resource_node(1), true);

	// Re-registering with a smaller amount keeps the larger recorded ceiling.
	state->register_resource_node(1, 50, t0);
	TESTEQUALS(state->is_resource_node(1), true);

	// A node whose entity has no LIVE component is pruned on the next tick.
	state->tick_resource_regen(t0 + FOREST_REGEN_INTERVAL_SEC);
	TESTEQUALS(state->is_resource_node(1), false);

	// Removing the entity clears its regeneration registration.
	state->register_resource_node(1, 100, t0);
	TESTEQUALS(state->is_resource_node(1), true);
	state->remove_game_entity(1);
	TESTEQUALS(state->is_resource_node(1), false);

	// When regeneration is disabled, ticking is a no-op and registrations persist.
	state->set_forest_regen_enabled(false);
	auto node2 = std::make_shared<GameEntity>(2);
	node2->add_component(std::make_shared<component::Position>(loop));
	state->add_game_entity(node2);
	state->register_resource_node(2, 100, t0);
	state->tick_resource_regen(t0 + FOREST_REGEN_INTERVAL_SEC * 10);
	TESTEQUALS(state->is_resource_node(2), true);
}

void streets_move_speed_multiplier() {
	auto loop = std::make_shared<openage::event::EventLoop>();
	auto db = nyan::Database::create();
	auto state = std::make_shared<GameState>(db, loop);

	coord::tile street{5, 5};
	coord::tile plain{6, 6};

	TESTEQUALS(state->is_streets_enabled(), false);
	TESTEQUALS(state->get_tile_move_speed_multiplier(street), 1.0);

	state->set_streets_enabled(true);
	state->register_street_tile(street, entity_id_t{42});
	TESTEQUALS(state->is_street_tile(street), true);
	TESTEQUALS(state->get_tile_move_speed_multiplier(street), STREET_MOVE_MULT);
	TESTEQUALS(state->get_tile_move_speed_multiplier(plain), 1.0);

	state->set_street_move_mult(1.5);
	TESTEQUALS(state->get_tile_move_speed_multiplier(street), 1.5);

	state->set_streets_enabled(false);
	TESTEQUALS(state->get_tile_move_speed_multiplier(street), 1.0);
}

void streets_lifecycle() {
	auto loop = std::make_shared<openage::event::EventLoop>();
	auto db = nyan::Database::create();
	auto state = std::make_shared<GameState>(db, loop);
	auto t0 = time::time_t::from_int(0);

	loop->add_event_handler(std::make_shared<gamestate::event::PlayerDefeatedHandler>());
	loop->add_event_handler(std::make_shared<gamestate::event::GameOverHandler>());

	state->set_streets_enabled(true);
	TESTEQUALS(state->can_place_street(coord::tile{3, 3}), true);

	auto building = std::make_shared<GameEntity>(7);
	auto ownership = std::make_shared<component::Ownership>(loop);
	ownership->set_owner(t0, player_id_t{0});
	building->add_component(ownership);
	state->add_game_entity(building);

	auto player = std::make_shared<Player>(player_id_t{0}, db->new_view(), loop);
	state->add_player(player);

	state->register_street_tile(coord::tile{3, 3}, 7);
	TESTEQUALS(state->is_street_tile(coord::tile{3, 3}), true);
	TESTEQUALS(state->can_place_street(coord::tile{3, 3}), false);

	state->remove_game_entity(7, t0);
	TESTEQUALS(state->is_street_tile(coord::tile{3, 3}), false);
}

void bridges_lifecycle() {
	auto loop = std::make_shared<openage::event::EventLoop>();
	auto db = nyan::Database::create();
	auto state = std::make_shared<GameState>(db, loop);
	auto t0 = time::time_t::from_int(0);

	loop->add_event_handler(std::make_shared<gamestate::event::PlayerDefeatedHandler>());
	loop->add_event_handler(std::make_shared<gamestate::event::GameOverHandler>());

	TESTEQUALS(state->is_bridges_enabled(), false);
	TESTEQUALS(state->can_place_bridge(coord::tile{2, 2}), false);

	state->set_bridges_enabled(true);
	TESTEQUALS(state->can_place_bridge(coord::tile{2, 2}), true);

	auto player = std::make_shared<Player>(player_id_t{0}, db->new_view(), loop);
	state->add_player(player);

	auto building = std::make_shared<GameEntity>(9);
	auto ownership = std::make_shared<component::Ownership>(loop);
	ownership->set_owner(t0, player_id_t{0});
	building->add_component(ownership);
	state->add_game_entity(building);

	state->register_bridge_tile(coord::tile{2, 2}, 9);
	TESTEQUALS(state->is_bridge_tile(coord::tile{2, 2}), true);
	TESTEQUALS(state->can_place_bridge(coord::tile{2, 2}), false);
	TESTEQUALS(state->can_place_street(coord::tile{2, 2}), false);

	// Bridge overlay is a no-op without Land/Water grids.
	state->apply_bridge_path_costs(path::grid_id_t{0}, t0);

	state->remove_game_entity(9, t0);
	TESTEQUALS(state->is_bridge_tile(coord::tile{2, 2}), false);
}

void building_kind_helpers() {
	TESTEQUALS(api::is_street_building("test.building.Street"), true);
	TESTEQUALS(api::is_street_building("aoe2_base.data.building.road.Road"), true);
	TESTEQUALS(api::is_street_building("test.building.House"), false);
	TESTEQUALS(api::is_bridge_building("test.building.WoodenBridge"), true);
	TESTEQUALS(api::is_bridge_building("test.building.Barracks"), false);
}

void population_no_phantom_release() {
	auto loop = std::make_shared<openage::event::EventLoop>();
	auto db = nyan::Database::create();
	auto state = std::make_shared<GameState>(db, loop);
	auto t0 = time::time_t::from_int(0);

	loop->add_event_handler(std::make_shared<gamestate::event::PlayerDefeatedHandler>());
	loop->add_event_handler(std::make_shared<gamestate::event::GameOverHandler>());

	auto player = std::make_shared<Player>(0, db->new_view(), loop);
	state->add_player(player);
	player->init_population(t0, 20);

	// Building without recorded ProvideContingent must not subtract default capacity.
	make_building(1, 0, loop, state, t0);
	// Keep a second building so destroy does not also defeat the player mid-assert.
	make_building(99, 0, loop, state, t0);
	state->remove_game_entity(1, t0);
	TESTEQUALS(player->get_population_capacity(t0), 20);

	// Recorded provision is still released correctly.
	state->set_entity_population_provision(99, 7);
	player->add_population_capacity(t0, 7);
	TESTEQUALS(player->get_population_capacity(t0), 27);
	state->remove_game_entity(99, t0);
	TESTEQUALS(player->get_population_capacity(t0), 20);
}

void street_tile_overwrite() {
	auto loop = std::make_shared<openage::event::EventLoop>();
	auto db = nyan::Database::create();
	auto state = std::make_shared<GameState>(db, loop);
	auto t0 = time::time_t::from_int(0);

	loop->add_event_handler(std::make_shared<gamestate::event::PlayerDefeatedHandler>());
	loop->add_event_handler(std::make_shared<gamestate::event::GameOverHandler>());
	auto player = std::make_shared<Player>(0, db->new_view(), loop);
	state->add_player(player);

	state->set_streets_enabled(true);
	make_building(1, 0, loop, state, t0);
	make_building(2, 0, loop, state, t0);

	coord::tile tile{4, 4};
	state->register_street_tile(tile, 1);
	state->register_street_tile(tile, 2); // overwrites owner 1
	TESTEQUALS(state->is_street_tile(tile), true);

	// Destroying the overwritten owner must not clear the tile still owned by 2.
	state->remove_game_entity(1, t0);
	TESTEQUALS(state->is_street_tile(tile), true);

	state->remove_game_entity(2, t0);
	TESTEQUALS(state->is_street_tile(tile), false);
}

void fog_last_known_cleared_on_remove() {
	auto loop = std::make_shared<openage::event::EventLoop>();
	auto db = nyan::Database::create();
	auto state = std::make_shared<GameState>(db, loop);
	auto t0 = time::time_t::from_int(0);

	auto observer = std::make_shared<Player>(player_id_t{0}, db->new_view(), loop);
	auto enemy_player = std::make_shared<Player>(player_id_t{1}, db->new_view(), loop);
	state->add_player(observer);
	state->add_player(enemy_player);

	auto scout = std::make_shared<GameEntity>(entity_id_t{1});
	auto scout_pos = std::make_shared<component::Position>(loop);
	scout_pos->set_position(t0, coord::phys3{10, 10, 0});
	scout->add_component(scout_pos);
	auto scout_own = std::make_shared<component::Ownership>(loop);
	scout_own->set_owner(t0, player_id_t{0});
	scout->add_component(scout_own);
	state->add_game_entity(scout);

	auto enemy = std::make_shared<GameEntity>(entity_id_t{2});
	auto enemy_pos = std::make_shared<component::Position>(loop);
	enemy_pos->set_position(t0, coord::phys3{12, 12, 0});
	enemy->add_component(enemy_pos);
	auto enemy_own = std::make_shared<component::Ownership>(loop);
	enemy_own->set_owner(t0, player_id_t{1});
	enemy->add_component(enemy_own);
	state->add_game_entity(enemy);

	state->refresh_visibility(t0);
	TESTEQUALS(state->is_entity_visible(player_id_t{0}, entity_id_t{2}, t0), true);
	TESTEQUALS(state->get_last_known_position(player_id_t{0}, entity_id_t{2}).has_value(), true);

	state->remove_game_entity(entity_id_t{2});
	TESTEQUALS(state->get_last_known_position(player_id_t{0}, entity_id_t{2}).has_value(), false);
}

void environment_day_night() {
	auto loop = std::make_shared<openage::event::EventLoop>();
	auto db = nyan::Database::create();
	auto state = std::make_shared<GameState>(db, loop);

	// Disabled by default: always daytime, full sight.
	TESTEQUALS(state->is_day_night_enabled(), false);
	TESTEQUALS(state->get_day_phase(time::time_t::from_int(0)) == day_phase_t::DAY, true);
	TESTEQUALS(state->get_sight_multiplier(time::time_t::from_int(0)), 1.0);

	state->set_day_night_enabled(true);
	state->set_day_night_params(100.0, 100.0);

	// Mid-day.
	TESTEQUALS(state->get_day_phase(time::time_t::from_double(40.0)) == day_phase_t::DAY, true);
	TESTEQUALS(state->get_sight_multiplier(time::time_t::from_double(40.0)), DAY_SIGHT_MULT);

	// Dusk: last 10% of day.
	TESTEQUALS(state->get_day_phase(time::time_t::from_double(95.0)) == day_phase_t::DUSK, true);
	TESTEQUALS(state->get_sight_multiplier(time::time_t::from_double(95.0)), TWILIGHT_SIGHT_MULT);

	// Night.
	TESTEQUALS(state->get_day_phase(time::time_t::from_double(140.0)) == day_phase_t::NIGHT, true);
	TESTEQUALS(state->get_sight_multiplier(time::time_t::from_double(140.0)), NIGHT_SIGHT_MULT);

	// Dawn: last 10% of night.
	TESTEQUALS(state->get_day_phase(time::time_t::from_double(195.0)) == day_phase_t::DAWN, true);
	TESTEQUALS(state->get_sight_multiplier(time::time_t::from_double(195.0)), TWILIGHT_SIGHT_MULT);
}

void environment_weather() {
	auto loop = std::make_shared<openage::event::EventLoop>();
	auto db = nyan::Database::create();
	auto state = std::make_shared<GameState>(db, loop);
	auto t0 = time::time_t::from_int(0);

	TESTEQUALS(state->is_weather_enabled(), false);
	TESTEQUALS(state->get_weather() == weather_t::CLEAR, true);
	TESTEQUALS(state->get_move_speed_multiplier(), WEATHER_CLEAR_MOVE_MULT);
	TESTEQUALS(state->get_sight_multiplier(t0), 1.0);

	state->set_weather_enabled(true);
	state->set_weather(weather_t::FOG);
	TESTEQUALS(state->get_weather() == weather_t::FOG, true);
	TESTEQUALS(state->get_sight_multiplier(t0), WEATHER_FOG_SIGHT_MULT);
	TESTEQUALS(state->get_move_speed_multiplier(), WEATHER_FOG_MOVE_MULT);

	state->set_weather(weather_t::RAIN);
	TESTEQUALS(state->get_sight_multiplier(t0), WEATHER_RAIN_SIGHT_MULT);
	TESTEQUALS(state->get_move_speed_multiplier(), WEATHER_RAIN_MOVE_MULT);

	// Weather cycles CLEAR -> FOG -> RAIN after the interval.
	state->set_weather(weather_t::CLEAR);
	state->tick_environment(t0);
	TESTEQUALS(state->get_weather() == weather_t::CLEAR, true);
	state->tick_environment(t0 + WEATHER_CYCLE_INTERVAL_SEC);
	TESTEQUALS(state->get_weather() == weather_t::FOG, true);
	state->tick_environment(t0 + WEATHER_CYCLE_INTERVAL_SEC * 2);
	TESTEQUALS(state->get_weather() == weather_t::RAIN, true);

	// Stacked with night: night * fog.
	state->set_day_night_enabled(true);
	state->set_day_night_params(100.0, 100.0);
	state->set_weather(weather_t::FOG);
	auto night = time::time_t::from_double(140.0);
	TESTEQUALS(state->get_sight_multiplier(night), NIGHT_SIGHT_MULT * WEATHER_FOG_SIGHT_MULT);
}

void environment_forest_hide() {
	auto loop = std::make_shared<openage::event::EventLoop>();
	auto db = nyan::Database::create();
	auto state = std::make_shared<GameState>(db, loop);
	auto t0 = time::time_t::from_int(0);

	TESTEQUALS(state->is_forest_hide_enabled(), false);

	auto observer = std::make_shared<Player>(player_id_t{0}, db->new_view(), loop);
	auto enemy_player = std::make_shared<Player>(player_id_t{1}, db->new_view(), loop);
	state->add_player(observer);
	state->add_player(enemy_player);

	auto make_unit = [&](entity_id_t id, player_id_t owner, coord::phys3 pos) {
		auto entity = std::make_shared<GameEntity>(id);
		auto position = std::make_shared<component::Position>(loop);
		position->set_position(t0, pos);
		entity->add_component(position);
		auto ownership = std::make_shared<component::Ownership>(loop);
		ownership->set_owner(t0, owner);
		entity->add_component(ownership);
		state->add_game_entity(entity);
	};

	// Scout at (10,10); enemy at (12,12) — within default sight range 4.
	make_unit(entity_id_t{1}, player_id_t{0}, coord::phys3{10, 10, 0});
	make_unit(entity_id_t{2}, player_id_t{1}, coord::phys3{12, 12, 0});

	coord::tile forest_tile{12, 12};
	state->mark_forest_tile(forest_tile);
	TESTEQUALS(state->is_forest_tile(forest_tile), true);

	state->refresh_visibility(t0);
	// Without forest hide, the enemy is fog-visible.
	TESTEQUALS(state->is_entity_visible(player_id_t{0}, entity_id_t{2}, t0), true);

	state->set_forest_hide_enabled(true);
	state->set_forest_hide_threshold(1);
	// Chebyshev distance from scout (10,10) to enemy (12,12) is 2 > threshold 1.
	TESTEQUALS(state->is_entity_visible(player_id_t{0}, entity_id_t{2}, t0), false);

	// Threshold 2 detects the enemy.
	state->set_forest_hide_threshold(2);
	TESTEQUALS(state->is_entity_visible(player_id_t{0}, entity_id_t{2}, t0), true);

	// Own units on forest tiles remain visible to themselves.
	state->mark_forest_tile(coord::tile{10, 10});
	TESTEQUALS(state->is_entity_visible(player_id_t{0}, entity_id_t{1}, t0), true);

	state->unmark_forest_tile(forest_tile);
	TESTEQUALS(state->is_forest_tile(forest_tile), false);
	state->clear_forest_tiles();
	TESTEQUALS(state->is_forest_tile(coord::tile{10, 10}), false);
}


void player_statistics() {
	auto loop = std::make_shared<openage::event::EventLoop>();
	auto db = nyan::Database::create();
	auto state = std::make_shared<GameState>(db, loop);
	auto view = db->new_view();
	auto t0 = time::time_t::from_int(0);

	auto player = std::make_shared<Player>(0, view, loop);
	state->add_player(player);

	// All counters start at zero.
	TESTEQUALS(player->get_units_killed(t0), 0);
	TESTEQUALS(player->get_units_lost(t0), 0);
	TESTEQUALS(player->get_total_resources_gathered(t0), 0);

	// Kills accumulate.
	player->record_kill(t0, 2);
	player->record_kill(t0);
	TESTEQUALS(player->get_units_killed(t0), 3);

	// Resources gathered accumulate per type and in total.
	player->record_resource_gathered(t0, "test.resource.Wood", 50);
	player->record_resource_gathered(t0, "test.resource.Gold", 30);
	player->record_resource_gathered(t0, "test.resource.Wood", 20);
	TESTEQUALS(player->get_resource_gathered(t0, "test.resource.Wood"), 70);
	TESTEQUALS(player->get_resource_gathered(t0, "test.resource.Gold"), 30);
	TESTEQUALS(player->get_resource_gathered(t0, "test.resource.Stone"), 0);
	TESTEQUALS(player->get_total_resources_gathered(t0), 100);

	// Actions accumulate and feed the APM calculation.
	TESTEQUALS(player->get_actions_issued(t0), 0);
	TESTEQUALS(player->get_apm(t0, 60.0), 0.0);
	player->record_action(t0);
	player->record_action(t0);
	player->record_action(t0);
	TESTEQUALS(player->get_actions_issued(t0), 3);
	// 3 actions over 30 s == 6 actions per minute.
	TESTEQUALS(player->get_apm(t0, 30.0), 6.0);
	// Zero elapsed time yields 0 (no division by zero).
	TESTEQUALS(player->get_apm(t0, 0.0), 0.0);

	// Destroying an owned entity records a loss via the (id, time) overload.
	loop->add_event_handler(std::make_shared<gamestate::event::PlayerDefeatedHandler>());
	loop->add_event_handler(std::make_shared<gamestate::event::GameOverHandler>());
	make_building(10, 0, loop, state, t0);
	make_building(11, 0, loop, state, t0);
	state->remove_game_entity(10, t0);
	TESTEQUALS(player->get_units_lost(t0), 1);
}

void entity_max_hp_tracking() {
	auto loop = std::make_shared<openage::event::EventLoop>();
	auto db = nyan::Database::create();
	auto state = std::make_shared<GameState>(db, loop);
	auto t0 = time::time_t::from_int(0);

	// Explicitly recorded max HP
	TESTEQUALS(state->has_entity_max_hp(1), false);
	state->set_entity_max_hp(1, 1500);
	TESTEQUALS(state->has_entity_max_hp(1), true);
	TESTEQUALS(state->get_entity_max_hp(1), 1500);

	// Fallback for unrecorded building (no MOVE) vs unit (has MOVE)
	auto building = std::make_shared<GameEntity>(10);
	auto pos = std::make_shared<component::Position>(loop);
	pos->set_position(t0, coord::phys3{0, 0, 0});
	building->add_component(pos);
	state->add_game_entity(building);
	TESTEQUALS(state->get_entity_max_hp(10), DEFAULT_BUILDING_MAX_HP);

	// Removal clears max HP tracking
	state->remove_game_entity(1, t0);
	TESTEQUALS(state->has_entity_max_hp(1), false);
}

void repair_command_lifecycle() {
	auto loop = std::make_shared<openage::event::EventLoop>();
	auto db = nyan::Database::create();
	auto state = std::make_shared<GameState>(db, loop);
	auto t0 = time::time_t::from_int(0);

	auto player = std::make_shared<Player>(0, db->new_view(), loop);
	state->add_player(player);
	player->add_resource(t0, "test.resource.Wood", 500);

	// Create damaged building at (0, 0, 0)
	auto building = std::make_shared<GameEntity>(10);
	auto b_pos = std::make_shared<component::Position>(loop);
	b_pos->set_position(t0, coord::phys3{0, 0, 0});
	building->add_component(b_pos);

	auto b_own = std::make_shared<component::Ownership>(loop);
	b_own->set_owner(t0, 0);
	building->add_component(b_own);

	auto b_live = std::make_shared<component::Live>(loop);
	b_live->add_attribute(
		time::TIME_MIN,
		"engine.ability.type.Live.AttributeAmount",
		std::make_shared<curve::Discrete<int64_t>>(loop, 0, "", nullptr, 500));
	building->add_component(b_live);
	state->add_game_entity(building);
	state->set_entity_max_hp(10, 1000);

	// Set building cost (100 Wood)
	BuildingCostRecord cost;
	cost.entries.push_back(ResourceCostEntry{"test.resource.Wood", 100});
	state->set_building_cost(10, cost);

	// Create builder at (0, 1, 0) (within REPAIR_INTERACTION_RANGE 2.0)
	auto builder = std::make_shared<GameEntity>(1);
	auto v_pos = std::make_shared<component::Position>(loop);
	v_pos->set_position(t0, coord::phys3{0, 1, 0});
	builder->add_component(v_pos);

	auto v_own = std::make_shared<component::Ownership>(loop);
	v_own->set_owner(t0, 0);
	builder->add_component(v_own);

	auto v_cmd = std::make_shared<component::CommandQueue>(loop);
	builder->add_component(v_cmd);
	state->add_game_entity(builder);

	// Issue repair command
	v_cmd->add_command(t0, std::make_shared<component::command::RepairCommand>(10));

	// Execute repair step
	auto dur = system::Repair::repair_command(builder, state, t0);
	TESTEQUALS(dur > time::time_t::from_int(0), true);

	// Verify HP increased from 500
	int64_t new_hp = b_live->get_attribute(t0, "engine.ability.type.Live.AttributeAmount");
	TESTEQUALS(new_hp > 500, true);

	// Verify wood was deducted proportionally
	int64_t wood = player->get_resource(t0, "test.resource.Wood");
	TESTEQUALS(wood < 500, true);

	// Verify repair command was re-enqueued for next tick
	TESTEQUALS(activity::next_command_repair(t0, builder), true);
	v_cmd->pop_command(t0);

	// Fully repaired building stops repair
	b_live->set_attribute(t0, "engine.ability.type.Live.AttributeAmount", 1000);
	v_cmd->add_command(t0, std::make_shared<component::command::RepairCommand>(10));
	auto dur_done = system::Repair::repair_command(builder, state, t0);
	TESTEQUALS(dur_done == time::time_t::from_int(0), true);
}

void repair_approach_movement() {
	auto loop = std::make_shared<openage::event::EventLoop>();
	auto db = nyan::Database::create();
	auto state = std::make_shared<GameState>(db, loop);
	auto t0 = time::time_t::from_int(0);

	auto building = std::make_shared<GameEntity>(10);
	auto b_pos = std::make_shared<component::Position>(loop);
	b_pos->set_position(t0, coord::phys3{20, 20, 0});
	building->add_component(b_pos);
	auto b_live = std::make_shared<component::Live>(loop);
	b_live->add_attribute(
		time::TIME_MIN,
		"engine.ability.type.Live.AttributeAmount",
		std::make_shared<curve::Discrete<int64_t>>(loop, 0, "", nullptr, 500));
	building->add_component(b_live);
	state->add_game_entity(building);

	// Builder far away at (0, 0, 0)
	auto builder = std::make_shared<GameEntity>(1);
	auto v_pos = std::make_shared<component::Position>(loop);
	v_pos->set_position(t0, coord::phys3{0, 0, 0});
	builder->add_component(v_pos);
	auto v_cmd = std::make_shared<component::CommandQueue>(loop);
	builder->add_component(v_cmd);
	state->add_game_entity(builder);

	// Without MOVE component, builder cannot approach; returns 0
	v_cmd->add_command(t0, std::make_shared<component::command::RepairCommand>(10));
	auto dur = system::Repair::repair_command(builder, state, t0);
	TESTEQUALS(dur == time::time_t::from_int(0), true);
}

void garrison_lifecycle() {
	auto loop = std::make_shared<openage::event::EventLoop>();
	auto db = nyan::Database::create();
	auto state = std::make_shared<GameState>(db, loop);
	auto t0 = time::time_t::from_int(0);

	auto player = std::make_shared<Player>(0, db->new_view(), loop);
	state->add_player(player);

	// Create building at (0, 0, 0)
	auto building = std::make_shared<GameEntity>(10);
	auto b_pos = std::make_shared<component::Position>(loop);
	b_pos->set_position(t0, coord::phys3{0, 0, 0});
	building->add_component(b_pos);

	auto b_own = std::make_shared<component::Ownership>(loop);
	b_own->set_owner(t0, 0);
	building->add_component(b_own);

	auto b_live = std::make_shared<component::Live>(loop);
	b_live->add_attribute(
		time::TIME_MIN,
		"engine.ability.type.Live.AttributeAmount",
		std::make_shared<curve::Discrete<int64_t>>(loop, 0, "", nullptr, 1000));
	building->add_component(b_live);
	state->add_game_entity(building);

	// Create unit at (0, 1, 0) (within GARRISON_INTERACTION_RANGE)
	auto unit = std::make_shared<GameEntity>(1);
	auto u_pos = std::make_shared<component::Position>(loop);
	u_pos->set_position(t0, coord::phys3{0, 1, 0});
	unit->add_component(u_pos);

	auto u_own = std::make_shared<component::Ownership>(loop);
	u_own->set_owner(t0, 0);
	unit->add_component(u_own);

	auto u_cmd = std::make_shared<component::CommandQueue>(loop);
	unit->add_component(u_cmd);
	state->add_game_entity(unit);

	TESTEQUALS(state->is_garrisoned(1), false);
	TESTEQUALS(state->can_garrison(1, 10), true);

	// Issue garrison command
	u_cmd->add_command(t0, std::make_shared<component::command::GarrisonCommand>(10));
	auto dur = system::Garrison::garrison_command(unit, state, t0);
	TESTEQUALS(dur == time::time_t::from_int(0), true);

	// Verify unit is garrisoned inside building 10
	TESTEQUALS(state->is_garrisoned(1), true);
	TESTEQUALS(state->get_garrison_parent(1).value(), 10);
	TESTEQUALS(state->get_garrisoned_units(10).size(), 1);

	// Ungarrison entities
	auto ejected = state->ungarrison_entities(10, t0);
	TESTEQUALS(ejected.size(), 1);
	TESTEQUALS(ejected[0], 1);
	TESTEQUALS(state->is_garrisoned(1), false);
	TESTEQUALS(state->get_garrisoned_units(10).empty(), true);
}

void garrison_capacity_and_ownership() {
	auto loop = std::make_shared<openage::event::EventLoop>();
	auto db = nyan::Database::create();
	auto state = std::make_shared<GameState>(db, loop);
	auto t0 = time::time_t::from_int(0);

	auto p0 = std::make_shared<Player>(0, db->new_view(), loop);
	auto p1 = std::make_shared<Player>(1, db->new_view(), loop);
	state->add_player(p0);
	state->add_player(p1);

	// Friendly building with capacity 1
	auto b = std::make_shared<GameEntity>(10);
	auto b_pos = std::make_shared<component::Position>(loop);
	b_pos->set_position(t0, coord::phys3{0, 0, 0});
	b->add_component(b_pos);
	auto b_own = std::make_shared<component::Ownership>(loop);
	b_own->set_owner(t0, 0);
	b->add_component(b_own);
	state->add_game_entity(b);
	state->set_garrison_capacity(10, 1);
	TESTEQUALS(state->get_garrison_capacity(10), 1);

	// Friendly unit 1
	auto u1 = std::make_shared<GameEntity>(1);
	auto u1_own = std::make_shared<component::Ownership>(loop);
	u1_own->set_owner(t0, 0);
	u1->add_component(u1_own);
	auto u1_pos = std::make_shared<component::Position>(loop);
	u1_pos->set_position(t0, coord::phys3{0, 0, 0});
	u1->add_component(u1_pos);
	state->add_game_entity(u1);

	// Friendly unit 2
	auto u2 = std::make_shared<GameEntity>(2);
	auto u2_own = std::make_shared<component::Ownership>(loop);
	u2_own->set_owner(t0, 0);
	u2->add_component(u2_own);
	auto u2_pos = std::make_shared<component::Position>(loop);
	u2_pos->set_position(t0, coord::phys3{0, 0, 0});
	u2->add_component(u2_pos);
	state->add_game_entity(u2);

	// Enemy unit 3 (owned by player 1)
	auto u3 = std::make_shared<GameEntity>(3);
	auto u3_own = std::make_shared<component::Ownership>(loop);
	u3_own->set_owner(t0, 1);
	u3->add_component(u3_own);
	auto u3_pos = std::make_shared<component::Position>(loop);
	u3_pos->set_position(t0, coord::phys3{0, 0, 0});
	u3->add_component(u3_pos);
	state->add_game_entity(u3);

	// Unit 1 can garrison
	TESTEQUALS(state->can_garrison(1, 10), true);
	state->garrison_entity(1, 10, t0);

	// Building is now full; unit 2 cannot garrison
	TESTEQUALS(state->can_garrison(2, 10), false);

	// Enemy unit 3 cannot garrison
	TESTEQUALS(state->can_garrison(3, 10), false);
}

void garrison_arrow_bonus_and_damage() {
	auto loop = std::make_shared<openage::event::EventLoop>();
	auto db = nyan::Database::create();
	auto state = std::make_shared<GameState>(db, loop);
	auto t0 = time::time_t::from_int(0);

	auto p0 = std::make_shared<Player>(0, db->new_view(), loop);
	auto p1 = std::make_shared<Player>(1, db->new_view(), loop);
	state->add_player(p0);
	state->add_player(p1);

	// Building with attack ability (damage = 5, reload = 2.0 -> DPS = 2.5)
	auto building = std::make_shared<GameEntity>(10);
	auto b_pos = std::make_shared<component::Position>(loop);
	b_pos->set_position(t0, coord::phys3{0, 0, 0});
	building->add_component(b_pos);
	auto b_own = std::make_shared<component::Ownership>(loop);
	b_own->set_owner(t0, 0);
	building->add_component(b_own);

	auto b_live = std::make_shared<component::Live>(loop);
	b_live->add_attribute(
		time::TIME_MIN,
		"engine.ability.type.Live.AttributeAmount",
		std::make_shared<curve::Discrete<int64_t>>(loop, 0, "", nullptr, 1000));
	building->add_component(b_live);

	auto b_atk = std::make_shared<component::Attack>(loop);
	b_atk->set_damage(std::make_shared<curve::Discrete<int64_t>>(loop, 0, "", nullptr, 5));
	b_atk->set_reload_time(std::make_shared<curve::Discrete<double>>(loop, 0, "", nullptr, 2.0));
	b_atk->set_max_range(std::make_shared<curve::Discrete<double>>(loop, 0, "", nullptr, 8.0));
	building->add_component(b_atk);
	state->add_game_entity(building);

	// Initially 0 extra arrows
	TESTEQUALS(state->get_building_additional_arrows(10, t0), 0);

	// Garrison 2 villagers (each contributes 2.5 DPS -> sum 5.0 DPS; 5.0 / 2.5 = 2 additional arrows)
	for (entity_id_t u_id = 1; u_id <= 2; ++u_id) {
		auto v = std::make_shared<GameEntity>(u_id);
		auto v_pos = std::make_shared<component::Position>(loop);
		v_pos->set_position(t0, coord::phys3{0, 0, 0});
		v->add_component(v_pos);
		auto v_own = std::make_shared<component::Ownership>(loop);
		v_own->set_owner(t0, 0);
		v->add_component(v_own);
		state->add_game_entity(v);
		state->garrison_entity(u_id, 10, t0);
	}

	TESTEQUALS(state->get_building_additional_arrows(10, t0), 2);

	// Create enemy target with 100 HP
	auto enemy = std::make_shared<GameEntity>(99);
	auto e_pos = std::make_shared<component::Position>(loop);
	e_pos->set_position(t0, coord::phys3{1, 1, 0});
	enemy->add_component(e_pos);
	auto e_own = std::make_shared<component::Ownership>(loop);
	e_own->set_owner(t0, 1);
	enemy->add_component(e_own);
	auto e_live = std::make_shared<component::Live>(loop);
	e_live->add_attribute(
		time::TIME_MIN,
		"engine.ability.type.Live.AttributeAmount",
		std::make_shared<curve::Discrete<int64_t>>(loop, 0, "", nullptr, 100));
	enemy->add_component(e_live);
	state->add_game_entity(enemy);

	// Attack enemy: deals base damage (5) + 2 extra arrows * 5 = 15 total damage
	system::Attack::attack_default(building, state, 99, t0);
	int64_t e_hp = e_live->get_attribute(t0, "engine.ability.type.Live.AttributeAmount");
	TESTEQUALS(e_hp, 85);
}

void town_bell_and_back_to_work() {
	auto loop = std::make_shared<openage::event::EventLoop>();
	auto db = nyan::Database::create();
	auto state = std::make_shared<GameState>(db, loop);
	auto t0 = time::time_t::from_int(0);

	auto p0 = std::make_shared<Player>(0, db->new_view(), loop);
	state->add_player(p0);

	// Town Center at (0, 0, 0)
	auto tc = std::make_shared<GameEntity>(10);
	auto tc_pos = std::make_shared<component::Position>(loop);
	tc_pos->set_position(t0, coord::phys3{0, 0, 0});
	tc->add_component(tc_pos);
	auto tc_own = std::make_shared<component::Ownership>(loop);
	tc_own->set_owner(t0, 0);
	tc->add_component(tc_own);
	state->add_game_entity(tc);

	// Villager 1 at (5, 5, 0) with a GatherCommand
	auto v1 = std::make_shared<GameEntity>(1);
	auto v1_pos = std::make_shared<component::Position>(loop);
	v1_pos->set_position(t0, coord::phys3{5, 5, 0});
	v1->add_component(v1_pos);
	auto v1_own = std::make_shared<component::Ownership>(loop);
	v1_own->set_owner(t0, 0);
	v1->add_component(v1_own);
	auto v1_move = std::make_shared<component::Move>(loop);
	v1->add_component(v1_move);
	auto v1_cmd = std::make_shared<component::CommandQueue>(loop);
	v1_cmd->add_command(t0, std::make_shared<component::command::GatherCommand>(900));
	v1->add_component(v1_cmd);
	state->add_game_entity(v1);

	// Villager 2 far away at (50, 50, 0) (outside 25 tiles) with a MoveCommand
	auto v2 = std::make_shared<GameEntity>(2);
	auto v2_pos = std::make_shared<component::Position>(loop);
	v2_pos->set_position(t0, coord::phys3{50, 50, 0});
	v2->add_component(v2_pos);
	auto v2_own = std::make_shared<component::Ownership>(loop);
	v2_own->set_owner(t0, 0);
	v2->add_component(v2_own);
	auto v2_move = std::make_shared<component::Move>(loop);
	v2->add_component(v2_move);
	auto v2_cmd = std::make_shared<component::CommandQueue>(loop);
	v2_cmd->add_command(t0, std::make_shared<component::command::MoveCommand>(coord::phys3{60, 60, 0}));
	v2->add_component(v2_cmd);
	state->add_game_entity(v2);

	// Ring Town Bell
	state->ring_town_bell(10, t0);

	// Villager 1 has GarrisonCommand in queue
	TESTEQUALS(activity::next_command_garrison(t0, v1), true);

	// Villager 2 is outside range and was not modified
	TESTEQUALS(activity::next_command_move(t0, v2), true);

	// Garrison Villager 1 directly
	state->garrison_entity(1, 10, t0);
	TESTEQUALS(state->is_garrisoned(1), true);

	// Ring bell again: Back to Work
	state->back_to_work(10, t0);

	// Villager 1 is ungarrisoned and its GatherCommand is restored!
	TESTEQUALS(state->is_garrisoned(1), false);
	TESTEQUALS(activity::next_command_gather(t0, v1), true);
}

void garrison_passive_heal() {
	auto loop = std::make_shared<openage::event::EventLoop>();
	auto db = nyan::Database::create();
	auto state = std::make_shared<GameState>(db, loop);
	auto t0 = time::time_t::from_int(0);

	auto building = std::make_shared<GameEntity>(10);
	auto b_pos = std::make_shared<component::Position>(loop);
	b_pos->set_position(t0, coord::phys3{0, 0, 0});
	building->add_component(b_pos);
	state->add_game_entity(building);

	auto unit = std::make_shared<GameEntity>(1);
	auto u_pos = std::make_shared<component::Position>(loop);
	u_pos->set_position(t0, coord::phys3{0, 0, 0});
	unit->add_component(u_pos);
	auto u_live = std::make_shared<component::Live>(loop);
	u_live->add_attribute(
		time::TIME_MIN,
		"engine.ability.type.Live.AttributeAmount",
		std::make_shared<curve::Discrete<int64_t>>(loop, 0, "", nullptr, 50));
	unit->add_component(u_live);
	state->add_game_entity(unit);
	state->set_entity_max_hp(1, 100);

	state->garrison_entity(1, 10, t0);

	// Advance 10 seconds: heals at 0.1 HP/s -> +1 HP
	state->tick_garrison_heal(t0, 10.0);
	int64_t healed_hp = u_live->get_attribute(t0, "engine.ability.type.Live.AttributeAmount");
	TESTEQUALS(healed_hp, 51);
}

void combat_melee_and_pierce_armor() {
	auto loop = std::make_shared<openage::event::EventLoop>();
	auto db = nyan::Database::create();
	auto state = std::make_shared<GameState>(db, loop);
	auto t0 = time::time_t::from_int(0);

	// Attacker: melee attack 10
	auto attacker = std::make_shared<GameEntity>(1);
	auto a_pos = std::make_shared<component::Position>(loop);
	a_pos->set_position(t0, coord::phys3{0, 0, 0});
	attacker->add_component(a_pos);
	auto a_atk = std::make_shared<component::Attack>(loop);
	a_atk->set_damage(t0, 10);
	a_atk->set_attack_type(t0, armor_class_t::MELEE);
	a_atk->set_max_range(t0, 2.0);
	attacker->add_component(a_atk);
	state->add_game_entity(attacker);

	// Defender: 100 HP, 3 melee armor
	auto defender = std::make_shared<GameEntity>(2);
	auto d_pos = std::make_shared<component::Position>(loop);
	d_pos->set_position(t0, coord::phys3{0, 1, 0});
	defender->add_component(d_pos);
	auto d_live = std::make_shared<component::Live>(loop);
	d_live->add_attribute(
		time::TIME_MIN,
		"engine.ability.type.Live.AttributeAmount",
		std::make_shared<curve::Discrete<int64_t>>(loop, 0, "", nullptr, 100));
	defender->add_component(d_live);
	state->add_game_entity(defender);
	state->set_entity_armor(2, armor_class_t::MELEE, 3);

	// Attack 1: deals 10 - 3 = 7 damage (100 -> 93 HP)
	system::Attack::attack_default(attacker, 2, state, t0);
	int64_t hp1 = d_live->get_attribute(t0, "engine.ability.type.Live.AttributeAmount");
	TESTEQUALS(hp1, 93);

	// Set extreme armor (20 armor vs 10 attack): AoE2 rule guarantees minimum 1 damage
	state->set_entity_armor(2, armor_class_t::MELEE, 20);
	system::Attack::attack_default(attacker, 2, state, t0);
	int64_t hp2 = d_live->get_attribute(t0, "engine.ability.type.Live.AttributeAmount");
	TESTEQUALS(hp2, 92);

	// Pierce attack test: switch attacker to pierce attack 8, defender pierce armor 2 -> 6 damage
	a_atk->set_attack_type(t0, armor_class_t::PIERCE);
	a_atk->set_damage(t0, 8);
	state->set_entity_armor(2, armor_class_t::PIERCE, 2);
	system::Attack::attack_default(attacker, 2, state, t0);
	int64_t hp3 = d_live->get_attribute(t0, "engine.ability.type.Live.AttributeAmount");
	TESTEQUALS(hp3, 86);
}

void combat_attack_bonuses() {
	auto loop = std::make_shared<openage::event::EventLoop>();
	auto db = nyan::Database::create();
	auto state = std::make_shared<GameState>(db, loop);
	auto t0 = time::time_t::from_int(0);

	// Spearman: base melee attack 3, +15 bonus vs CAVALRY
	auto spearman = std::make_shared<GameEntity>(1);
	auto s_pos = std::make_shared<component::Position>(loop);
	s_pos->set_position(t0, coord::phys3{0, 0, 0});
	spearman->add_component(s_pos);
	auto s_atk = std::make_shared<component::Attack>(loop);
	s_atk->set_damage(t0, 3);
	s_atk->set_attack_type(t0, armor_class_t::MELEE);
	s_atk->set_max_range(t0, 2.0);
	spearman->add_component(s_atk);
	state->add_game_entity(spearman);
	state->set_entity_attack_bonus(1, armor_class_t::CAVALRY, 15);

	// Target 1: Knight (100 HP, 2 melee armor, 0 cavalry armor resist)
	auto knight = std::make_shared<GameEntity>(2);
	auto k_pos = std::make_shared<component::Position>(loop);
	k_pos->set_position(t0, coord::phys3{0, 1, 0});
	knight->add_component(k_pos);
	auto k_live = std::make_shared<component::Live>(loop);
	k_live->add_attribute(
		time::TIME_MIN,
		"engine.ability.type.Live.AttributeAmount",
		std::make_shared<curve::Discrete<int64_t>>(loop, 0, "", nullptr, 100));
	knight->add_component(k_live);
	state->add_game_entity(knight);
	state->set_entity_armor(2, armor_class_t::MELEE, 2);
	state->set_entity_armor(2, armor_class_t::CAVALRY, 0);

	// Attack knight: (3 - 2 base) + (15 - 0 bonus) = 16 damage (100 -> 84 HP)
	system::Attack::attack_default(spearman, 2, state, t0);
	int64_t k_hp = k_live->get_attribute(t0, "engine.ability.type.Live.AttributeAmount");
	TESTEQUALS(k_hp, 84);

	// Target 2: Archer (100 HP, 0 melee armor, does NOT have CAVALRY class -> unpossessed default 1000)
	auto archer = std::make_shared<GameEntity>(3);
	auto ar_pos = std::make_shared<component::Position>(loop);
	ar_pos->set_position(t0, coord::phys3{0, 1, 0});
	archer->add_component(ar_pos);
	auto ar_live = std::make_shared<component::Live>(loop);
	ar_live->add_attribute(
		time::TIME_MIN,
		"engine.ability.type.Live.AttributeAmount",
		std::make_shared<curve::Discrete<int64_t>>(loop, 0, "", nullptr, 100));
	archer->add_component(ar_live);
	state->add_game_entity(archer);
	// Does not set CAVALRY armor on archer

	// Attack archer: (3 - 0 base) + 0 bonus = 3 damage (100 -> 97 HP)
	system::Attack::attack_default(spearman, 3, state, t0);
	int64_t ar_hp = ar_live->get_attribute(t0, "engine.ability.type.Live.AttributeAmount");
	TESTEQUALS(ar_hp, 97);
}

void combat_elevation_modifier() {
	auto loop = std::make_shared<openage::event::EventLoop>();
	auto db = nyan::Database::create();
	auto state = std::make_shared<GameState>(db, loop);
	auto t0 = time::time_t::from_int(0);

	// Attacker on high ground: z = 1.0, attack 10
	auto attacker = std::make_shared<GameEntity>(1);
	auto a_pos = std::make_shared<component::Position>(loop);
	a_pos->set_position(t0, coord::phys3{0, 0, coord::phys_t{1.0}});
	attacker->add_component(a_pos);
	auto a_atk = std::make_shared<component::Attack>(loop);
	a_atk->set_damage(t0, 10);
	a_atk->set_max_range(t0, 5.0);
	attacker->add_component(a_atk);
	state->add_game_entity(attacker);

	// Defender on low ground: z = 0.0, 100 HP, 0 armor
	auto defender = std::make_shared<GameEntity>(2);
	auto d_pos = std::make_shared<component::Position>(loop);
	d_pos->set_position(t0, coord::phys3{1, 0, coord::phys_t{0.0}});
	defender->add_component(d_pos);
	auto d_live = std::make_shared<component::Live>(loop);
	d_live->add_attribute(
		time::TIME_MIN,
		"engine.ability.type.Live.AttributeAmount",
		std::make_shared<curve::Discrete<int64_t>>(loop, 0, "", nullptr, 100));
	defender->add_component(d_live);
	state->add_game_entity(defender);

	// High ground attack: 10 * 1.25 = 12.5 -> round to 13 damage (100 -> 87 HP)
	system::Attack::attack_default(attacker, 2, state, t0);
	int64_t d_hp = d_live->get_attribute(t0, "engine.ability.type.Live.AttributeAmount");
	TESTEQUALS(d_hp, 87);

	// Low ground unit attacking high ground defender: 10 * 0.75 = 7.5 -> round to 8 damage
	auto uphill_attacker = std::make_shared<GameEntity>(3);
	auto u_pos = std::make_shared<component::Position>(loop);
	u_pos->set_position(t0, coord::phys3{2, 0, coord::phys_t{0.0}});
	uphill_attacker->add_component(u_pos);
	auto u_atk = std::make_shared<component::Attack>(loop);
	u_atk->set_damage(t0, 10);
	u_atk->set_max_range(t0, 5.0);
	uphill_attacker->add_component(u_atk);
	state->add_game_entity(uphill_attacker);

	// Target is unit 1 (at z = 1.0)
	auto a_live = std::make_shared<component::Live>(loop);
	a_live->add_attribute(
		time::TIME_MIN,
		"engine.ability.type.Live.AttributeAmount",
		std::make_shared<curve::Discrete<int64_t>>(loop, 0, "", nullptr, 100));
	attacker->add_component(a_live);

	system::Attack::attack_default(uphill_attacker, 1, state, t0);
	int64_t a_hp = a_live->get_attribute(t0, "engine.ability.type.Live.AttributeAmount");
	TESTEQUALS(a_hp, 92);
}

void combat_minimum_range() {
	auto loop = std::make_shared<openage::event::EventLoop>();
	auto db = nyan::Database::create();
	auto state = std::make_shared<GameState>(db, loop);
	auto t0 = time::time_t::from_int(0);

	// Attacker (e.g. Mangonel / Skirmisher) with min_range 3.0, max_range 8.0
	auto attacker = std::make_shared<GameEntity>(1);
	auto a_pos = std::make_shared<component::Position>(loop);
	a_pos->set_position(t0, coord::phys3{0, 0, 0});
	attacker->add_component(a_pos);
	auto a_atk = std::make_shared<component::Attack>(loop);
	a_atk->set_damage(t0, 20);
	a_atk->set_min_range(t0, 3.0);
	a_atk->set_max_range(t0, 8.0);
	attacker->add_component(a_atk);
	state->add_game_entity(attacker);

	// Target too close: dist = 1.0 < 3.0 min_range
	auto target = std::make_shared<GameEntity>(2);
	auto t_pos = std::make_shared<component::Position>(loop);
	t_pos->set_position(t0, coord::phys3{0, 1, 0});
	target->add_component(t_pos);
	auto t_live = std::make_shared<component::Live>(loop);
	t_live->add_attribute(
		time::TIME_MIN,
		"engine.ability.type.Live.AttributeAmount",
		std::make_shared<curve::Discrete<int64_t>>(loop, 0, "", nullptr, 100));
	target->add_component(t_live);
	state->add_game_entity(target);

	// Attack fails due to min range; target HP remains 100
	system::Attack::attack_default(attacker, 2, state, t0);
	int64_t hp = t_live->get_attribute(t0, "engine.ability.type.Live.AttributeAmount");
	TESTEQUALS(hp, 100);

	// Move target to dist = 4.0 >= 3.0 min_range
	t_pos->set_position(t0, coord::phys3{0, 4, 0});
	system::Attack::attack_default(attacker, 2, state, t0);
	int64_t hp_after = t_live->get_attribute(t0, "engine.ability.type.Live.AttributeAmount");
	TESTEQUALS(hp_after, 80);
}

void combat_siege_splash_damage() {
	auto loop = std::make_shared<openage::event::EventLoop>();
	auto db = nyan::Database::create();
	auto state = std::make_shared<GameState>(db, loop);
	auto t0 = time::time_t::from_int(0);

	// Siege unit: damage 40, blast_radius 2.0
	auto catapult = std::make_shared<GameEntity>(1);
	auto c_pos = std::make_shared<component::Position>(loop);
	c_pos->set_position(t0, coord::phys3{0, 0, 0});
	catapult->add_component(c_pos);
	auto c_atk = std::make_shared<component::Attack>(loop);
	c_atk->set_damage(t0, 40);
	c_atk->set_blast_radius(t0, 2.0);
	c_atk->set_max_range(t0, 10.0);
	catapult->add_component(c_atk);
	state->add_game_entity(catapult);

	// Primary target at (5, 0, 0)
	auto target1 = std::make_shared<GameEntity>(10);
	auto t1_pos = std::make_shared<component::Position>(loop);
	t1_pos->set_position(t0, coord::phys3{5, 0, 0});
	target1->add_component(t1_pos);
	auto t1_live = std::make_shared<component::Live>(loop);
	t1_live->add_attribute(
		time::TIME_MIN,
		"engine.ability.type.Live.AttributeAmount",
		std::make_shared<curve::Discrete<int64_t>>(loop, 0, "", nullptr, 100));
	target1->add_component(t1_live);
	state->add_game_entity(target1);

	// Secondary target at (5, 1, 0) (dist 1.0 from primary target <= 2.0 blast radius)
	auto target2 = std::make_shared<GameEntity>(20);
	auto t2_pos = std::make_shared<component::Position>(loop);
	t2_pos->set_position(t0, coord::phys3{5, 1, 0});
	target2->add_component(t2_pos);
	auto t2_live = std::make_shared<component::Live>(loop);
	t2_live->add_attribute(
		time::TIME_MIN,
		"engine.ability.type.Live.AttributeAmount",
		std::make_shared<curve::Discrete<int64_t>>(loop, 0, "", nullptr, 50));
	target2->add_component(t2_live);
	state->add_game_entity(target2);

	// Distant target at (5, 8, 0) (dist 8.0 from primary target > 2.0 blast radius)
	auto target3 = std::make_shared<GameEntity>(30);
	auto t3_pos = std::make_shared<component::Position>(loop);
	t3_pos->set_position(t0, coord::phys3{5, 8, 0});
	target3->add_component(t3_pos);
	auto t3_live = std::make_shared<component::Live>(loop);
	t3_live->add_attribute(
		time::TIME_MIN,
		"engine.ability.type.Live.AttributeAmount",
		std::make_shared<curve::Discrete<int64_t>>(loop, 0, "", nullptr, 50));
	target3->add_component(t3_live);
	state->add_game_entity(target3);

	// Fire catapult at target1
	system::Attack::attack_default(catapult, 10, state, t0);

	// Primary target took full 40 damage (100 -> 60)
	int64_t hp1 = t1_live->get_attribute(t0, "engine.ability.type.Live.AttributeAmount");
	TESTEQUALS(hp1, 60);

	// Secondary target took splash damage: 40 * (1 - 1/(2*2)) = 40 * 0.75 = 30 dmg (50 -> 20)
	int64_t hp2 = t2_live->get_attribute(t0, "engine.ability.type.Live.AttributeAmount");
	TESTEQUALS(hp2, 20);

	// Distant target took no splash damage (50 HP)
	int64_t hp3 = t3_live->get_attribute(t0, "engine.ability.type.Live.AttributeAmount");
	TESTEQUALS(hp3, 50);
}

void market_commodity_buy_and_sell() {
	auto loop = std::make_shared<openage::event::EventLoop>();
	auto db = nyan::Database::create();
	auto state = std::make_shared<GameState>(db, loop);
	auto t0 = time::time_t::from_int(0);

	auto player1 = std::make_shared<Player>(1, state->get_db_view(), loop);
	state->add_player(player1);

	// Verify initial base prices
	TESTEQUALS(state->get_market_base_price(market_resource_t::WOOD), 100);
	TESTEQUALS(state->get_market_base_price(market_resource_t::FOOD), 100);
	TESTEQUALS(state->get_market_base_price(market_resource_t::STONE), 130);

	// Initial buy and sell prices with 30% fee
	TESTEQUALS(state->get_market_buy_price(1, market_resource_t::WOOD), 130);
	TESTEQUALS(state->get_market_sell_price(1, market_resource_t::WOOD), 70);
	TESTEQUALS(state->get_market_buy_price(1, market_resource_t::STONE), 169);
	TESTEQUALS(state->get_market_sell_price(1, market_resource_t::STONE), 91);

	// Give Player 1 500 gold and 200 wood
	std::string gold_fqon = state->get_market_resource_fqon(market_resource_t::GOLD);
	std::string wood_fqon = state->get_market_resource_fqon(market_resource_t::WOOD);
	player1->add_resource(t0, gold_fqon, 500);
	player1->add_resource(t0, wood_fqon, 200);

	// Player 1 sells 100 wood: gains 70 gold, loses 100 wood; base drops from 100 to 97
	bool sold = state->market_sell(1, market_resource_t::WOOD, 1, t0);
	TESTEQUALS(sold, true);
	TESTEQUALS(player1->get_resource(t0, wood_fqon), 100);
	TESTEQUALS(player1->get_resource(t0, gold_fqon), 570);
	TESTEQUALS(state->get_market_base_price(market_resource_t::WOOD), 97);

	// New buy price with base 97: floor(97 * 1.30 + 0.5) = 126
	TESTEQUALS(state->get_market_buy_price(1, market_resource_t::WOOD), 126);

	// Player 1 buys 100 wood: pays 126 gold (570 -> 444), receives 100 wood (100 -> 200); base increments back to 100
	bool bought = state->market_buy(1, market_resource_t::WOOD, 1, t0);
	TESTEQUALS(bought, true);
	TESTEQUALS(player1->get_resource(t0, wood_fqon), 200);
	TESTEQUALS(player1->get_resource(t0, gold_fqon), 444);
	TESTEQUALS(state->get_market_base_price(market_resource_t::WOOD), 100);
}

void market_dynamic_price_fluctuations() {
	auto loop = std::make_shared<openage::event::EventLoop>();
	auto db = nyan::Database::create();
	auto state = std::make_shared<GameState>(db, loop);
	auto t0 = time::time_t::from_int(0);

	auto player = std::make_shared<Player>(1, state->get_db_view(), loop);
	state->add_player(player);

	std::string wood_fqon = state->get_market_resource_fqon(market_resource_t::WOOD);
	std::string gold_fqon = state->get_market_resource_fqon(market_resource_t::GOLD);
	player->add_resource(t0, wood_fqon, 10000);
	player->add_resource(t0, gold_fqon, 100000);

	// Selling 30 batches of 100 wood: 100 - (30 * 3) = 10 -> clamped to floor 20
	bool sold = state->market_sell(1, market_resource_t::WOOD, 30, t0);
	TESTEQUALS(sold, true);
	TESTEQUALS(state->get_market_base_price(market_resource_t::WOOD), 20);

	// At base price 20, sell price is floor(20 * 0.70 + 0.5) = 14
	TESTEQUALS(state->get_market_sell_price(1, market_resource_t::WOOD), 14);

	// Selling further cannot lower base price below 20
	state->market_sell(1, market_resource_t::WOOD, 5, t0);
	TESTEQUALS(state->get_market_base_price(market_resource_t::WOOD), 20);

	// Set base price near ceiling and test clamp
	state->set_market_base_price(market_resource_t::WOOD, 9998);
	TESTEQUALS(state->get_market_base_price(market_resource_t::WOOD), 9998);
	state->market_buy(1, market_resource_t::WOOD, 2, t0);
	TESTEQUALS(state->get_market_base_price(market_resource_t::WOOD), 9999);
}

void market_fee_customization() {
	auto loop = std::make_shared<openage::event::EventLoop>();
	auto db = nyan::Database::create();
	auto state = std::make_shared<GameState>(db, loop);

	auto player1 = std::make_shared<Player>(1, state->get_db_view(), loop);
	auto player2 = std::make_shared<Player>(2, state->get_db_view(), loop);
	state->add_player(player1);
	state->add_player(player2);

	// Player 1 researched Guilds: fee reduced to 15%
	state->set_market_fee(1, 0.15);
	TESTEQUALS(state->get_market_buy_price(1, market_resource_t::WOOD), 115);
	TESTEQUALS(state->get_market_sell_price(1, market_resource_t::WOOD), 85);

	// Player 2 is Saracens: civ bonus sets fee to 5%
	state->set_market_fee(2, 0.05);
	TESTEQUALS(state->get_market_buy_price(2, market_resource_t::WOOD), 105);
	TESTEQUALS(state->get_market_sell_price(2, market_resource_t::WOOD), 95);
}

void market_tribute_system_and_fees() {
	auto loop = std::make_shared<openage::event::EventLoop>();
	auto db = nyan::Database::create();
	auto state = std::make_shared<GameState>(db, loop);
	auto t0 = time::time_t::from_int(0);

	auto p1 = std::make_shared<Player>(1, state->get_db_view(), loop);
	auto p2 = std::make_shared<Player>(2, state->get_db_view(), loop);
	state->add_player(p1);
	state->add_player(p2);

	std::string food_fqon = state->get_market_resource_fqon(market_resource_t::FOOD);
	p1->add_resource(t0, food_fqon, 500);
	p2->add_resource(t0, food_fqon, 0);

	// Tribute 100 food from p1 to p2 with default 30% tax: sender pays 130, recipient receives 100
	bool trib1 = state->send_tribute(1, 2, food_fqon, 100, t0);
	TESTEQUALS(trib1, true);
	TESTEQUALS(p1->get_resource(t0, food_fqon), 370);
	TESTEQUALS(p2->get_resource(t0, food_fqon), 100);
	TESTEQUALS(p1->get_tribute_sent(t0, food_fqon), 100);
	TESTEQUALS(p2->get_tribute_received(t0, food_fqon), 100);

	// Attempting to send 300 food when cost is 300 + 90 = 390 > 370 must fail
	bool trib_fail = state->send_tribute(1, 2, food_fqon, 300, t0);
	TESTEQUALS(trib_fail, false);
	TESTEQUALS(p1->get_resource(t0, food_fqon), 370);
	TESTEQUALS(p2->get_resource(t0, food_fqon), 100);

	// Player 1 researched Banking: tribute tax reduced to 10%
	state->set_tribute_fee(1, 0.10);
	// Sending 300 food now costs 300 + 30 = 330 <= 370: succeeds!
	bool trib2 = state->send_tribute(1, 2, food_fqon, 300, t0);
	TESTEQUALS(trib2, true);
	TESTEQUALS(p1->get_resource(t0, food_fqon), 40);
	TESTEQUALS(p2->get_resource(t0, food_fqon), 400);
	TESTEQUALS(p1->get_tribute_sent(t0, food_fqon), 400);
}

void trade_cart_gold_distance_formula() {
	auto loop = std::make_shared<openage::event::EventLoop>();
	auto db = nyan::Database::create();
	auto state = std::make_shared<GameState>(db, loop);

	coord::phys3 p1{0, 0, 0};
	coord::phys3 p2{0, 0, 0};
	// Zero distance yields guaranteed minimum 1 gold
	int64_t gold_zero = state->calculate_trade_gold(p1, p2, 120.0);
	TESTEQUALS(gold_zero, 1);

	// Markets 60 tiles apart in x and y: dx=60, dy=60 -> d=sqrt(55^2 + 55^2)=77.78
	// gold = floor(2 * (77.78/120 + 0.3) * 77.78 + 0.5) = 147
	coord::phys3 p3{10, 10, 0};
	coord::phys3 p4{70, 70, 0};
	int64_t gold_dist = state->calculate_trade_gold(p3, p4, 120.0);
	TESTEQUALS(gold_dist, 147);
}

void trade_cart_continuous_trading_loop_and_market_destruction() {
	auto loop = std::make_shared<openage::event::EventLoop>();
	auto db = nyan::Database::create();
	auto state = std::make_shared<GameState>(db, loop);
	auto t0 = time::time_t::from_int(0);

	auto player1 = std::make_shared<Player>(1, state->get_db_view(), loop);
	state->add_player(player1);

	// Home market at (0, 0, 0)
	auto home_market = std::make_shared<GameEntity>(10);
	auto hm_pos = std::make_shared<component::Position>(loop);
	hm_pos->set_position(t0, coord::phys3{0, 0, 0});
	home_market->add_component(hm_pos);
	auto hm_own = std::make_shared<component::Ownership>(loop, 1);
	home_market->add_component(hm_own);
	state->add_game_entity(home_market);
	state->register_market_entity(10);

	// Target market at (6, 0, 0)
	auto target_market = std::make_shared<GameEntity>(20);
	auto tm_pos = std::make_shared<component::Position>(loop);
	tm_pos->set_position(t0, coord::phys3{6, 0, 0});
	target_market->add_component(tm_pos);
	auto tm_own = std::make_shared<component::Ownership>(loop, 2);
	target_market->add_component(tm_own);
	state->add_game_entity(target_market);
	state->register_market_entity(20);

	// Trade cart at (0, 0, 0)
	auto trade_cart = std::make_shared<GameEntity>(30);
	auto tc_pos = std::make_shared<component::Position>(loop);
	tc_pos->set_position(t0, coord::phys3{0, 0, 0});
	trade_cart->add_component(tc_pos);
	auto tc_own = std::make_shared<component::Ownership>(loop, 1);
	trade_cart->add_component(tc_own);
	auto tc_queue = std::make_shared<component::CommandQueue>(loop);
	tc_queue->add_command(t0, std::make_shared<component::command::TradeCommand>(20, 10));
	trade_cart->add_component(tc_queue);
	state->add_game_entity(trade_cart);

	// Cart at (0,0) is out of range of target market at (6,0): steps and re-enqueues
	system::Trade::trade_command(trade_cart, state, t0);
	TESTEQUALS(tc_queue->get_queue().empty(t0), false);
	auto cmd1 = std::dynamic_pointer_cast<component::command::TradeCommand>(tc_queue->get_queue().front(t0));
	TESTEQUALS(cmd1->get_state() == component::command::trade_state_t::TO_TARGET, true);

	// Move cart to within interaction range of target market (e.g. at 5.0)
	tc_pos->set_position(t0, coord::phys3{5.0, 0, 0});
	system::Trade::trade_command(trade_cart, state, t0);

	// Reached target market: gold cargo loaded, state flipped to RETURNING_HOME
	TESTEQUALS(state->is_carrying_resources(30), true);
	auto cargo = state->get_carried_resource(30);
	TESTEQUALS(cargo->amount > 0, true);
	auto cmd2 = std::dynamic_pointer_cast<component::command::TradeCommand>(tc_queue->get_queue().front(t0));
	TESTEQUALS(cmd2->get_state() == component::command::trade_state_t::RETURNING_HOME, true);

	// Move cart back to within interaction range of home market (e.g. at 0.5)
	tc_pos->set_position(t0, coord::phys3{0.5, 0, 0});
	system::Trade::trade_command(trade_cart, state, t0);

	// Reached home market: cargo unloaded and credited to player gold, trade profit recorded, flips to TO_TARGET
	TESTEQUALS(state->is_carrying_resources(30), false);
	std::string gold_fqon = state->get_market_resource_fqon(market_resource_t::GOLD);
	TESTEQUALS(player1->get_resource(t0, gold_fqon), cargo->amount);
	TESTEQUALS(player1->get_trade_profit(t0), cargo->amount);
	auto cmd3 = std::dynamic_pointer_cast<component::command::TradeCommand>(tc_queue->get_queue().front(t0));
	TESTEQUALS(cmd3->get_state() == component::command::trade_state_t::TO_TARGET, true);

	// Destroy target market
	state->remove_game_entity(20, t0);
	TESTEQUALS(state->is_market_entity(20), false);

	// Cart runs next trade command: target is destroyed and cart holds no cargo -> stops trading
	system::Trade::trade_command(trade_cart, state, t0);
	TESTEQUALS(tc_queue->get_queue().empty(t0), true);
}

void player_age_progression_lifecycle() {
	auto loop = std::make_shared<openage::event::EventLoop>();
	auto db = nyan::Database::create();
	auto state = std::make_shared<GameState>(db, loop);
	auto t0 = time::time_t::from_int(0);
	auto t1 = time::time_t::from_int(500);
	auto t2 = time::time_t::from_int(1200);
	auto t3 = time::time_t::from_int(2000);

	auto player = std::make_shared<Player>(1, state->get_db_view(), loop);
	state->add_player(player);

	// Starts in Dark Age
	TESTEQUALS(player->get_age(t0) == age_t::DARK_AGE, true);
	TESTEQUALS(player->get_tech_count(), 0);

	// Progress through ages over time
	player->set_age(t1, age_t::FEUDAL_AGE);
	player->set_age(t2, age_t::CASTLE_AGE);
	player->set_age(t3, age_t::IMPERIAL_AGE);

	// Check historical queries
	TESTEQUALS(player->get_age(t0) == age_t::DARK_AGE, true);
	TESTEQUALS(player->get_age(t1) == age_t::FEUDAL_AGE, true);
	TESTEQUALS(player->get_age(t2) == age_t::CASTLE_AGE, true);
	TESTEQUALS(player->get_age(t3) == age_t::IMPERIAL_AGE, true);
	TESTEQUALS(player->get_age(time::time_t::from_int(3000)) == age_t::IMPERIAL_AGE, true);
}

void research_queue_cost_and_prerequisites() {
	auto loop = std::make_shared<openage::event::EventLoop>();
	auto db = nyan::Database::create();
	auto state = std::make_shared<GameState>(db, loop);
	auto t0 = time::time_t::from_int(0);

	auto player = std::make_shared<Player>(1, state->get_db_view(), loop);
	state->add_player(player);

	// Town center building
	auto tc = std::make_shared<GameEntity>(10);
	auto own = std::make_shared<component::Ownership>(loop, 1);
	tc->add_component(own);
	state->add_game_entity(tc);

	// Second building
	auto tc2 = std::make_shared<GameEntity>(11);
	auto own2 = std::make_shared<component::Ownership>(loop, 1);
	tc2->add_component(own2);
	state->add_game_entity(tc2);

	// Cannot research Castle Age while in Dark Age (wrong required age)
	TESTEQUALS(state->can_research(1, 10, TECH_CASTLE_AGE, t0), false);

	// Cannot research Feudal Age with 0 food
	std::string food_fqon = "test.resource.Food";
	TESTEQUALS(state->can_research(1, 10, TECH_FEUDAL_AGE, t0), false);
	TESTEQUALS(state->start_research(1, 10, TECH_FEUDAL_AGE, t0), false);

	// Give 500 food
	player->add_resource(t0, food_fqon, FEUDAL_AGE_COST_FOOD);
	TESTEQUALS(player->get_resource(t0, food_fqon), FEUDAL_AGE_COST_FOOD);

	// Can now research Feudal Age
	TESTEQUALS(state->can_research(1, 10, TECH_FEUDAL_AGE, t0), true);
	TESTEQUALS(state->start_research(1, 10, TECH_FEUDAL_AGE, t0), true);

	// Food is deducted
	TESTEQUALS(player->get_resource(t0, food_fqon), 0);

	// Building 10 has active research
	TESTEQUALS(state->is_researching(10), true);
	auto active = state->get_active_research(10);
	TESTEQUALS(active.has_value(), true);
	TESTEQUALS(active->tech_id, TECH_FEUDAL_AGE);

	// Feudal Age is in progress for player 1
	TESTEQUALS(state->is_tech_in_progress(1, TECH_FEUDAL_AGE), true);

	// Building 10 cannot start another research while busy
	TESTEQUALS(state->can_research(1, 10, TECH_LOOM, t0), false);

	// Building 11 cannot start researching Feudal Age (already in progress for player 1)
	player->add_resource(t0, food_fqon, FEUDAL_AGE_COST_FOOD);
	TESTEQUALS(state->can_research(1, 11, TECH_FEUDAL_AGE, t0), false);
}

void research_cancellation_and_full_refund() {
	auto loop = std::make_shared<openage::event::EventLoop>();
	auto db = nyan::Database::create();
	auto state = std::make_shared<GameState>(db, loop);
	auto t0 = time::time_t::from_int(0);

	auto player = std::make_shared<Player>(1, state->get_db_view(), loop);
	state->add_player(player);

	auto tc = std::make_shared<GameEntity>(10);
	auto own = std::make_shared<component::Ownership>(loop, 1);
	tc->add_component(own);
	state->add_game_entity(tc);

	std::string food_fqon = "test.resource.Food";
	player->add_resource(t0, food_fqon, FEUDAL_AGE_COST_FOOD);

	TESTEQUALS(state->start_research(1, 10, TECH_FEUDAL_AGE, t0), true);
	TESTEQUALS(player->get_resource(t0, food_fqon), 0);
	TESTEQUALS(state->is_researching(10), true);
	TESTEQUALS(state->is_tech_in_progress(1, TECH_FEUDAL_AGE), true);

	// Cancel research
	TESTEQUALS(state->cancel_research(10, t0), true);

	// 100% refund given back to player
	TESTEQUALS(player->get_resource(t0, food_fqon), FEUDAL_AGE_COST_FOOD);
	TESTEQUALS(state->is_researching(10), false);
	TESTEQUALS(state->is_tech_in_progress(1, TECH_FEUDAL_AGE), false);

	// Player can immediately start researching again
	TESTEQUALS(state->start_research(1, 10, TECH_FEUDAL_AGE, t0), true);
	TESTEQUALS(player->get_resource(t0, food_fqon), 0);
}

void research_completion_and_age_advancement() {
	auto loop = std::make_shared<openage::event::EventLoop>();
	auto db = nyan::Database::create();
	auto state = std::make_shared<GameState>(db, loop);
	auto t0 = time::time_t::from_int(0);

	auto player = std::make_shared<Player>(1, state->get_db_view(), loop);
	state->add_player(player);

	auto tc = std::make_shared<GameEntity>(10);
	auto own = std::make_shared<component::Ownership>(loop, 1);
	tc->add_component(own);
	state->add_game_entity(tc);

	std::string food_fqon = "test.resource.Food";
	player->add_resource(t0, food_fqon, FEUDAL_AGE_COST_FOOD);

	// Start Feudal Age research (130 seconds)
	TESTEQUALS(state->start_research(1, 10, TECH_FEUDAL_AGE, t0), true);

	// At t = 100s, research is still in progress
	auto t100 = time::time_t::from_int(100);
	auto completed_early = state->take_completed_researches(t100);
	TESTEQUALS(completed_early.empty(), true);
	TESTEQUALS(state->is_researching(10), true);
	TESTEQUALS(player->get_age(t100) == age_t::DARK_AGE, true);
	TESTEQUALS(player->has_researched(TECH_FEUDAL_AGE), false);

	// At t = 135s (>= 130s), research completes
	auto t135 = time::time_t::from_int(135);
	state->tick_research(t135);

	TESTEQUALS(state->is_researching(10), false);
	TESTEQUALS(state->is_tech_in_progress(1, TECH_FEUDAL_AGE), false);
	TESTEQUALS(player->has_researched(TECH_FEUDAL_AGE), true);
	TESTEQUALS(player->get_age(t135) == age_t::FEUDAL_AGE, true);
	TESTEQUALS(player->get_tech_count(), 1);

	// Already researched -> cannot research Feudal Age again
	player->add_resource(t135, food_fqon, FEUDAL_AGE_COST_FOOD);
	TESTEQUALS(state->can_research(1, 10, TECH_FEUDAL_AGE, t135), false);
}

void technology_effect_application() {
	auto loop = std::make_shared<openage::event::EventLoop>();
	auto db = nyan::Database::create();
	auto state = std::make_shared<GameState>(db, loop);
	auto t0 = time::time_t::from_int(0);

	auto player = std::make_shared<Player>(1, state->get_db_view(), loop);
	// Set age to Castle Age so Coinage can be researched
	player->set_age(t0, age_t::CASTLE_AGE);
	state->add_player(player);

	auto market = std::make_shared<GameEntity>(10);
	auto own = std::make_shared<component::Ownership>(loop, 1);
	market->add_component(own);
	state->add_game_entity(market);

	// Starting tribute fee is 30%
	TESTEQUALS(state->get_tribute_fee(1), TRIBUTE_DEFAULT_FEE);

	// Research Coinage
	std::string food_fqon = "test.resource.Food";
	std::string gold_fqon = "test.resource.Gold";
	player->add_resource(t0, food_fqon, 500);
	player->add_resource(t0, gold_fqon, 500);

	TESTEQUALS(state->start_research(1, 10, TECH_COINAGE, t0), true);
	auto t_done = t0 + 100.0;
	state->tick_research(t_done);

	// Coinage effect reduces tribute fee to 20%
	TESTEQUALS(player->has_researched(TECH_COINAGE), true);
	TESTEQUALS(state->get_tribute_fee(1), TRIBUTE_FEE_COINAGE);

	// Advance to Imperial Age and research Banking and Guilds
	player->set_age(t_done, age_t::IMPERIAL_AGE);
	TESTEQUALS(state->start_research(1, 10, TECH_BANKING, t_done), true);
	state->tick_research(t_done + 100.0);
	TESTEQUALS(player->has_researched(TECH_BANKING), true);
	TESTEQUALS(state->get_tribute_fee(1), TRIBUTE_FEE_BANKING);

	// Research Guilds -> market fee reduced from 30% to 15%
	TESTEQUALS(state->get_market_fee(1), MARKET_DEFAULT_FEE);
	TESTEQUALS(state->start_research(1, 10, TECH_GUILDS, t_done + 100.0), true);
	state->tick_research(t_done + 200.0);
	TESTEQUALS(player->has_researched(TECH_GUILDS), true);
	TESTEQUALS(state->get_market_fee(1), MARKET_FEE_GUILDS);
}

void research_building_destruction_handling() {
	auto loop = std::make_shared<openage::event::EventLoop>();
	auto db = nyan::Database::create();
	auto state = std::make_shared<GameState>(db, loop);
	auto t0 = time::time_t::from_int(0);

	auto player = std::make_shared<Player>(1, state->get_db_view(), loop);
	state->add_player(player);

	auto tc1 = std::make_shared<GameEntity>(10);
	auto own1 = std::make_shared<component::Ownership>(loop, 1);
	tc1->add_component(own1);
	state->add_game_entity(tc1);

	auto tc2 = std::make_shared<GameEntity>(20);
	auto own2 = std::make_shared<component::Ownership>(loop, 1);
	tc2->add_component(own2);
	state->add_game_entity(tc2);

	std::string gold_fqon = "test.resource.Gold";
	player->add_resource(t0, gold_fqon, LOOM_COST_GOLD * 2);

	// Start researching Loom in TC1
	TESTEQUALS(state->start_research(1, 10, TECH_LOOM, t0), true);
	TESTEQUALS(state->is_researching(10), true);
	TESTEQUALS(state->is_tech_in_progress(1, TECH_LOOM), true);

	// TC2 cannot research Loom while in progress
	TESTEQUALS(state->can_research(1, 20, TECH_LOOM, t0), false);

	// Destroy TC1 (e.g. killed by enemy)
	state->remove_game_entity(10, t0);

	// TC1 is gone, research is cleared and lock is lifted
	TESTEQUALS(state->is_researching(10), false);
	TESTEQUALS(state->is_tech_in_progress(1, TECH_LOOM), false);
	TESTEQUALS(player->has_researched(TECH_LOOM), false);

	// In AoE2, destroyed building loses the cost (no refund)
	TESTEQUALS(player->get_resource(t0, gold_fqon), LOOM_COST_GOLD);

	// Player can now research Loom in TC2!
	TESTEQUALS(state->can_research(1, 20, TECH_LOOM, t0), true);
	TESTEQUALS(state->start_research(1, 20, TECH_LOOM, t0), true);
	TESTEQUALS(state->is_researching(20), true);
}

} // namespace openage::gamestate::tests
