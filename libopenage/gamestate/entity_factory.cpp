// Copyright 2023-2026 the openage authors. See copying.md for legal info.

#include "entity_factory.h"

#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <unordered_set>

#include "error/error.h"

#include "curve/container/queue.h"
#include "curve/discrete.h"
#include "event/event_loop.h"
#include "gamestate/activity/activity.h"
#include "gamestate/activity/condition/command_in_queue.h"
#include "gamestate/activity/condition/next_command.h"
#include "gamestate/activity/end_node.h"
#include "gamestate/activity/event/command_in_queue.h"
#include "gamestate/activity/event/wait.h"
#include "gamestate/activity/start_node.h"
#include "gamestate/activity/task_system_node.h"
#include "gamestate/activity/xor_event_gate.h"
#include "gamestate/activity/xor_gate.h"
#include "gamestate/api/activity.h"
#include "gamestate/component/api/idle.h"
#include "gamestate/component/api/attack.h"
#include "gamestate/component/api/create.h"
#include "gamestate/component/api/gather.h"
#include "gamestate/component/api/live.h"
#include "gamestate/component/api/move.h"
#include "gamestate/component/api/selectable.h"
#include "gamestate/component/api/turn.h"
#include "gamestate/component/internal/activity.h"
#include "gamestate/component/internal/command_queue.h"
#include "gamestate/component/internal/ownership.h"
#include "gamestate/component/internal/position.h"
#include "gamestate/component/internal/stance.h"
#include "gamestate/component/types.h"
#include "gamestate/game_entity.h"
#include "gamestate/game_state.h"
#include "gamestate/manager.h"
#include "gamestate/player.h"
#include "gamestate/system/types.h"
#include "log/message.h"
#include "renderer/render_factory.h"
#include "time/time.h"
#include "util/fixed_point.h"

namespace openage::gamestate {

/**
 * Create a simple test activity for the game entity.
 *
 * The activity is as follows:
 *
 *                              |----------------------------(new cmd)-----|
 *                              |                                          v
 * Start -> Idle -> Capable? -> Command? -> CmdType? -> Move       -> WaitMove       -> (loop to idle)
 *                    |           |            |
 *                    |           |            +-> Attack      -> Idle (loop)
 *                    |           |            |
 *                    |           |            +-> Gather      -> WaitGather      -> (loop to idle)
 *                    |           |            |
 *                    |           |            +-> Create      -> WaitCreate      -> (loop to idle)
 *                    |           |            |
 *                    |           |            +-> AttackMove  -> WaitAttackMove  -> (loop to idle)
 *                    |           |            |
 *                    |           |            +-> Patrol      -> WaitPatrol      -> (loop to idle)
 *                    |           |            |
 *                    |           |            +-> Guard       -> WaitGuard       -> (loop to idle)
 *                    |           |            |
 *                    |           |            +-> FormMove    -> WaitFormMove    -> (loop to idle)
 *                    |           |
 *                    |           +-> WaitForCmd -> CmdType?
 *                    |
 *                    +-> (none of MOVE/ATTACK/GATHER/CREATE/ATTACK_MOVE/PATROL/GUARD/FORMOVE) -> End
 *
 * TODO: Replace with config
 */
std::shared_ptr<activity::Activity> create_test_activity() {
	// Node IDs
	auto start = std::make_shared<activity::StartNode>(0);
	auto idle = std::make_shared<activity::TaskSystemNode>(1, "Idle");
	auto condition_capable = std::make_shared<activity::XorGate>(2);
	auto condition_command = std::make_shared<activity::XorGate>(3);
	auto condition_cmd_type = std::make_shared<activity::XorGate>(4);
	auto wait_for_command = std::make_shared<activity::XorEventGate>(5);
	auto move = std::make_shared<activity::TaskSystemNode>(6, "Move");
	auto wait_for_move = std::make_shared<activity::XorEventGate>(7);
	auto attack = std::make_shared<activity::TaskSystemNode>(8, "Attack");
	auto gather = std::make_shared<activity::TaskSystemNode>(9, "Gather");
	auto wait_for_gather = std::make_shared<activity::XorEventGate>(10);
	auto create = std::make_shared<activity::TaskSystemNode>(11, "Create");
	auto wait_for_create = std::make_shared<activity::XorEventGate>(12);
	auto build = std::make_shared<activity::TaskSystemNode>(22, "Build");
	auto wait_for_build = std::make_shared<activity::XorEventGate>(23);
	auto end = std::make_shared<activity::EndNode>(13);
	auto attack_move = std::make_shared<activity::TaskSystemNode>(14, "AttackMove");
	auto wait_for_attack_move = std::make_shared<activity::XorEventGate>(15);
	auto patrol = std::make_shared<activity::TaskSystemNode>(16, "Patrol");
	auto wait_for_patrol = std::make_shared<activity::XorEventGate>(17);
	auto guard = std::make_shared<activity::TaskSystemNode>(18, "Guard");
	auto wait_for_guard = std::make_shared<activity::XorEventGate>(19);
	auto formation_move = std::make_shared<activity::TaskSystemNode>(20, "FormationMove");
	auto wait_for_formation_move = std::make_shared<activity::XorEventGate>(21);

	start->add_output(idle);

	// idle after start
	idle->add_output(condition_capable);
	idle->set_system_id(system::system_id_t::IDLE);

	// Check if entity can move, attack, gather, or produce; end if none
	activity::condition_t capable_branch = [](const time::time_t & /* time */,
	                                          const std::shared_ptr<gamestate::GameEntity> &entity) {
		return entity->has_component(component::component_t::MOVE)
		       || entity->has_component(component::component_t::ATTACK)
		       || entity->has_component(component::component_t::GATHER)
		       || entity->has_component(component::component_t::CREATE);
	};
	condition_capable->add_output(condition_command, capable_branch);
	condition_capable->set_default(end);

	// Check if there is already a command in the queue
	condition_command->add_output(condition_cmd_type, gamestate::activity::command_in_queue);
	condition_command->set_default(wait_for_command);

	// Wait for a command, then check its type
	wait_for_command->add_output(condition_cmd_type, gamestate::activity::primer_command_in_queue);

	// Route to specific behaviour based on next command type
	condition_cmd_type->add_output(attack, gamestate::activity::next_command_attack);
	condition_cmd_type->add_output(gather, gamestate::activity::next_command_gather);
	condition_cmd_type->add_output(create, gamestate::activity::next_command_train);
	condition_cmd_type->add_output(build, gamestate::activity::next_command_build);
	condition_cmd_type->add_output(attack_move, gamestate::activity::next_command_attack_move);
	condition_cmd_type->add_output(patrol, gamestate::activity::next_command_patrol);
	condition_cmd_type->add_output(guard, gamestate::activity::next_command_guard);
	condition_cmd_type->add_output(formation_move, gamestate::activity::next_command_formation_move);
	condition_cmd_type->set_default(move);

	// Move system node
	move->add_output(wait_for_move);
	move->set_system_id(system::system_id_t::MOVE_COMMAND);

	// After move: wait for completion, then loop back to idle;
	// interrupt on a new command to re-evaluate command type
	wait_for_move->add_output(idle, gamestate::activity::primer_wait);
	wait_for_move->add_output(condition_cmd_type, gamestate::activity::primer_command_in_queue);

	// Attack system node: one attack per pass, then back to idle
	attack->add_output(idle);
	attack->set_system_id(system::system_id_t::ATTACK_COMMAND);

	// Gather system node: one gather action per pass, then wait for reload
	gather->add_output(wait_for_gather);
	gather->set_system_id(system::system_id_t::GATHER_COMMAND);

	// After gather: wait for reload time, then loop back to idle;
	// interrupt on a new command to re-evaluate
	wait_for_gather->add_output(idle, gamestate::activity::primer_wait);
	wait_for_gather->add_output(condition_cmd_type, gamestate::activity::primer_command_in_queue);

	// Create (train) system node: start producing one unit, then wait for the
	// creation time before looping back to idle
	create->add_output(wait_for_create);
	create->set_system_id(system::system_id_t::TRAIN_COMMAND);

	// After create: wait for the creation time, then loop back to idle;
	// interrupt on a new command to re-evaluate
	wait_for_create->add_output(idle, gamestate::activity::primer_wait);
	wait_for_create->add_output(condition_cmd_type, gamestate::activity::primer_command_in_queue);

	build->add_output(wait_for_build);
	build->set_system_id(system::system_id_t::BUILD_COMMAND);

	wait_for_build->add_output(idle, gamestate::activity::primer_wait);
	wait_for_build->add_output(condition_cmd_type, gamestate::activity::primer_command_in_queue);

	// Attack-move system node: move toward destination, attacking enemies along the way
	attack_move->add_output(wait_for_attack_move);
	attack_move->set_system_id(system::system_id_t::ATTACK_MOVE_COMMAND);

	wait_for_attack_move->add_output(idle, gamestate::activity::primer_wait);
	wait_for_attack_move->add_output(condition_cmd_type, gamestate::activity::primer_command_in_queue);

	// Patrol system node: cycle between waypoints, attacking enemies along the way
	patrol->add_output(wait_for_patrol);
	patrol->set_system_id(system::system_id_t::PATROL_COMMAND);

	wait_for_patrol->add_output(idle, gamestate::activity::primer_wait);
	wait_for_patrol->add_output(condition_cmd_type, gamestate::activity::primer_command_in_queue);

	// Guard system node: follow a target and attack nearby threats
	guard->add_output(wait_for_guard);
	guard->set_system_id(system::system_id_t::GUARD_COMMAND);

	wait_for_guard->add_output(idle, gamestate::activity::primer_wait);
	wait_for_guard->add_output(condition_cmd_type, gamestate::activity::primer_command_in_queue);

	// Formation-move system node: move while preserving formation offsets
	formation_move->add_output(wait_for_formation_move);
	formation_move->set_system_id(system::system_id_t::FORMATION_MOVE_COMMAND);

	wait_for_formation_move->add_output(idle, gamestate::activity::primer_wait);
	wait_for_formation_move->add_output(condition_cmd_type, gamestate::activity::primer_command_in_queue);

	return std::make_shared<activity::Activity>(0, start, "test");
}

EntityFactory::EntityFactory() :
	next_entity_id{0},
	next_player_id{0},
	render_factory{nullptr} {
}

std::shared_ptr<GameEntity> EntityFactory::add_game_entity(const std::shared_ptr<openage::event::EventLoop> &loop,
                                                           const std::shared_ptr<GameState> &state,
                                                           player_id_t owner_id,
                                                           const nyan::fqon_t &nyan_entity) {
	auto entity = std::make_shared<GameEntity>(this->get_next_entity_id());
	entity->set_manager(std::make_shared<GameEntityManager>(loop, state, entity));

	// use the owner's data to initialize the entity
	// this ensures that only the owner's tech upgrades apply
	auto db_view = state->get_player(owner_id)->get_db_view();
	init_components(loop, db_view, entity, nyan_entity);

	if (this->render_factory) {
		entity->set_render_entity(this->render_factory->add_world_render_entity());
	}

	return entity;
}

std::shared_ptr<Player> EntityFactory::add_player(const std::shared_ptr<openage::event::EventLoop> &loop,
                                                  const std::shared_ptr<GameState> &state,
                                                  const nyan::fqon_t & /* player_setup */) {
	auto player = std::make_shared<Player>(this->get_next_player_id(),
	                                       state->get_db_view()->new_child(),
	                                       loop);

	// TODO: Components (for resources, diplomacy, etc.)

	return player;
}

void EntityFactory::attach_renderer(const std::shared_ptr<renderer::RenderFactory> &render_factory) {
	std::unique_lock lock{this->mutex};

	this->render_factory = render_factory;
}

void EntityFactory::init_components(const std::shared_ptr<openage::event::EventLoop> &loop,
                                    const std::shared_ptr<nyan::View> &owner_db_view,
                                    const std::shared_ptr<GameEntity> &entity,
                                    const nyan::fqon_t &nyan_entity) {
	auto position = std::make_shared<component::Position>(loop);
	entity->add_component(position);

	auto ownership = std::make_shared<component::Ownership>(loop);
	entity->add_component(ownership);

	auto command_queue = std::make_shared<component::CommandQueue>(loop);
	entity->add_component(command_queue);

	// All entities start with AGGRESSIVE stance; can be changed via SET_STANCE command.
	auto stance = std::make_shared<component::Stance>(loop);
	entity->add_component(stance);

	auto nyan_obj = owner_db_view->get_object(nyan_entity);
	nyan::set_t abilities = nyan_obj.get_set("GameEntity.abilities");

	std::optional<nyan::Object> activity_ability;
	for (const auto &ability_val : abilities) {
		auto ability_fqon = std::dynamic_pointer_cast<nyan::ObjectValue>(ability_val.get_ptr())->get_name();
		auto ability_obj = owner_db_view->get_object(ability_fqon);

		auto ability_parent = ability_obj.get_parents()[0];
		if (ability_parent == "engine.ability.type.Move") {
			auto move = std::make_shared<component::Move>(loop, ability_obj);
			entity->add_component(move);
		}
		else if (ability_parent == "engine.ability.type.Turn") {
			auto turn = std::make_shared<component::Turn>(loop, ability_obj);
			entity->add_component(turn);
		}
		else if (ability_parent == "engine.ability.type.Idle") {
			auto idle = std::make_shared<component::Idle>(loop, ability_obj);
			entity->add_component(idle);
		}
		else if (ability_parent == "engine.ability.type.Live") {
			auto live = std::make_shared<component::Live>(loop, ability_obj);
			entity->add_component(live);

			auto attr_settings = ability_obj.get_set("Live.attributes");
			for (auto &setting : attr_settings) {
				auto setting_obj_val = std::dynamic_pointer_cast<nyan::ObjectValue>(setting.get_ptr());
				auto setting_obj = owner_db_view->get_object(setting_obj_val->get_name());
				auto attribute = setting_obj.get_object("AttributeSetting.attribute");
				auto start_value = setting_obj.get_int("AttributeSetting.starting_value");

				live->add_attribute(time::TIME_MIN,
				                    attribute.get_name(),
				                    std::make_shared<curve::Discrete<int64_t>>(loop,
				                                                               0,
				                                                               "",
				                                                               nullptr,
				                                                               start_value));
			}
		}
		else if (ability_parent == "engine.ability.type.Activity") {
			activity_ability = ability_obj;
		}
		else if (ability_parent == "engine.ability.type.Attack") {
			auto attack_comp = std::make_shared<component::Attack>(loop, ability_obj);
			entity->add_component(attack_comp);
		}
		else if (ability_parent == "engine.ability.type.Gather") {
			auto gather_comp = std::make_shared<component::Gather>(loop, ability_obj);
			entity->add_component(gather_comp);
		}
		else if (ability_parent == "engine.ability.type.Create") {
			auto create_comp = std::make_shared<component::Create>(loop, ability_obj);
			entity->add_component(create_comp);
		}
		else if (ability_parent == "engine.ability.type.Selectable") {
			auto selectable = std::make_shared<component::Selectable>(loop, ability_obj);
			entity->add_component(selectable);
		}
	}

	if (activity_ability) {
		init_activity(loop, owner_db_view, entity, activity_ability.value());
	}
	else {
		auto activity = std::make_shared<component::Activity>(loop, create_test_activity());
		entity->add_component(activity);
	}
}

void EntityFactory::init_activity(const std::shared_ptr<openage::event::EventLoop> &loop,
                                  const std::shared_ptr<nyan::View> &owner_db_view,
                                  const std::shared_ptr<GameEntity> &entity,
                                  const nyan::Object &ability) {
	nyan::Object graph = ability.get_object("Activity.graph");

	// Check if the activity is already exists in the cache
	if (this->activity_cache.contains(graph.get_name())) {
		auto activity = this->activity_cache.at(graph.get_name());
		auto component = std::make_shared<component::Activity>(loop, activity);
		entity->add_component(component);

		return;
	}

	auto start_obj = api::APIActivity::get_start(graph);

	size_t node_id = 0;

	std::deque<nyan::Object> nyan_nodes;
	std::unordered_map<size_t, std::shared_ptr<activity::Node>> node_id_map{};
	std::unordered_map<nyan::fqon_t, size_t> visited{};
	std::shared_ptr<activity::Node> start_node;

	// First pass: create all nodes using breadth-first search
	nyan_nodes.push_back(start_obj);
	while (!nyan_nodes.empty()) {
		auto node = nyan_nodes.front();
		nyan_nodes.pop_front();

		if (visited.contains(node.get_name())) {
			continue;
		}

		// Create the node
		switch (api::APIActivityNode::get_type(node)) {
		case activity::node_t::END:
			break;
		case activity::node_t::START:
			start_node = std::make_shared<activity::StartNode>(node_id);
			node_id_map[node_id] = start_node;
			break;
		case activity::node_t::TASK_SYSTEM: {
			auto task_node = std::make_shared<activity::TaskSystemNode>(node_id);
			task_node->set_system_id(api::APIActivityNode::get_system_id(node));
			node_id_map[node_id] = task_node;
			break;
		}
		case activity::node_t::XOR_GATE:
			node_id_map[node_id] = std::make_shared<activity::XorGate>(node_id);
			break;
		case activity::node_t::XOR_EVENT_GATE:
			node_id_map[node_id] = std::make_shared<activity::XorEventGate>(node_id);
			break;
		default:
			throw Error{ERR << "Unknown activity node type of node: " << node.get_name()};
		}

		// Get the node's outputs
		auto next_nodes = api::APIActivityNode::get_next(node);
		nyan_nodes.insert(nyan_nodes.end(), next_nodes.begin(), next_nodes.end());

		visited.insert({node.get_name(), node_id});
		node_id++;
	}

	// Second pass: connect the nodes
	for (const auto &current_node : visited) {
		auto nyan_node = owner_db_view->get_object(current_node.first);
		auto activity_node = node_id_map[current_node.second];

		switch (activity_node->get_type()) {
		case activity::node_t::END:
			break;
		case activity::node_t::START: {
			auto start = std::static_pointer_cast<activity::StartNode>(activity_node);
			auto output_fqon = nyan_node.get<nyan::ObjectValue>("Start.next")->get_name();
			auto output_id = visited[output_fqon];
			auto output_node = node_id_map[output_id];
			start->add_output(output_node);
			break;
		}
		case activity::node_t::TASK_SYSTEM: {
			auto task_system = std::static_pointer_cast<activity::TaskSystemNode>(activity_node);
			auto output_fqon = nyan_node.get<nyan::ObjectValue>("Ability.next")->get_name();
			auto output_id = visited[output_fqon];
			auto output_node = node_id_map[output_id];
			task_system->add_output(output_node);
			break;
		}
		case activity::node_t::XOR_GATE: {
			auto xor_gate = std::static_pointer_cast<activity::XorGate>(activity_node);
			auto conditions = nyan_node.get<nyan::OrderedSet>("XORGate.next");
			for (auto &condition : conditions->get()) {
				auto condition_value = std::dynamic_pointer_cast<nyan::ObjectValue>(condition.get_ptr());
				auto condition_obj = owner_db_view->get_object(condition_value->get_name());

				auto output_value = condition_obj.get<nyan::ObjectValue>("Condition.next")->get_name();
				auto output_id = visited[output_value];
				auto output_node = node_id_map[output_id];

				xor_gate->add_output(output_node, api::APIActivityCondition::get_condition(condition_obj));
			}

			auto default_fqon = nyan_node.get<nyan::ObjectValue>("XORGate.default")->get_name();
			auto default_id = visited[default_fqon];
			auto default_node = node_id_map[default_id];
			xor_gate->set_default(default_node);
			break;
		}
		case activity::node_t::XOR_EVENT_GATE: {
			auto xor_event_gate = std::static_pointer_cast<activity::XorEventGate>(activity_node);
			auto next = nyan_node.get<nyan::Dict>("XOREventGate.next");
			for (auto &next_node : next->get()) {
				auto event_value = std::dynamic_pointer_cast<nyan::ObjectValue>(next_node.first.get_ptr());
				auto event_obj = owner_db_view->get_object(event_value->get_name());

				auto next_node_value = std::dynamic_pointer_cast<nyan::ObjectValue>(next_node.second.get_ptr());
				auto next_node_obj = owner_db_view->get_object(next_node_value->get_name());

				auto output_id = visited[next_node_obj.get_name()];
				auto output_node = node_id_map[output_id];

				xor_event_gate->add_output(output_node, api::APIActivityEvent::get_primer(event_obj));
			}
			break;
		}
		default:
			throw Error{ERR << "Unknown activity node type of node: " << current_node.first};
		}
	}

	auto activity = std::make_shared<activity::Activity>(0, start_node, graph.get_name());
	this->activity_cache.insert({graph.get_name(), activity});

	auto component = std::make_shared<component::Activity>(loop, activity);
	entity->add_component(component);
}

entity_id_t EntityFactory::get_next_entity_id() {
	auto new_id = this->next_entity_id;
	this->next_entity_id++;

	return new_id;
}

player_id_t EntityFactory::get_next_player_id() {
	auto new_id = this->next_player_id;
	this->next_player_id++;

	return new_id;
}

} // namespace openage::gamestate
