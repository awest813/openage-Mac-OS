// Copyright 2025-2025 the openage authors. See copying.md for legal info.

#include "../testing/testing.h"

#include <memory>
#include <vector>

#include <nyan/nyan.h>

#include "event/eventhandler.h"
#include "event/event_loop.h"
#include "activity/condition/next_command.h"
#include "component/internal/command_queue.h"
#include "component/internal/commands/attack.h"
#include "component/internal/commands/gather.h"
#include "component/internal/commands/idle.h"
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
}

} // namespace openage::gamestate::tests
