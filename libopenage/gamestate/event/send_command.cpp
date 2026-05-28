// Copyright 2023-2026 the openage authors. See copying.md for legal info.

#include "send_command.h"

#include <cmath>
#include <string>
#include <vector>

#include "coord/phys.h"
#include "gamestate/component/internal/command_queue.h"
#include "gamestate/component/internal/commands/attack.h"
#include "gamestate/component/internal/commands/attack_move.h"
#include "gamestate/component/internal/commands/formation_move.h"
#include "gamestate/component/internal/commands/gather.h"
#include "gamestate/component/internal/commands/guard.h"
#include "gamestate/component/internal/commands/idle.h"
#include "gamestate/component/internal/commands/move.h"
#include "gamestate/component/internal/commands/patrol.h"
#include "gamestate/component/internal/commands/train.h"
#include "gamestate/component/internal/position.h"
#include "gamestate/component/internal/stance.h"
#include "gamestate/component/types.h"
#include "gamestate/game_entity.h"
#include "gamestate/game_state.h"
#include "gamestate/types.h"


namespace openage::gamestate {
namespace component {
class CommandQueue;

namespace command {
class IdleCommand;
class MoveCommand;
} // namespace command
} // namespace component

namespace event {

Commander::Commander(const std::shared_ptr<openage::event::EventLoop> &loop) :
	openage::event::EventEntity{loop} {
}

size_t Commander::id() const {
	return 0;
}

std::string Commander::idstr() const {
	return "command_target";
}


SendCommandHandler::SendCommandHandler() :
	openage::event::OnceEventHandler{"game.send_command"} {
}

void SendCommandHandler::setup_event(const std::shared_ptr<openage::event::Event> & /* event */,
                                     const std::shared_ptr<openage::event::State> & /* state */) {
	// TODO
}

void SendCommandHandler::invoke(openage::event::EventLoop & /* loop */,
                                const std::shared_ptr<openage::event::EventEntity> & /* target */,
                                const std::shared_ptr<openage::event::State> &state,
                                const time::time_t &time,
                                const param_map &params) {
	auto gstate = std::dynamic_pointer_cast<openage::gamestate::GameState>(state);

	auto command_type = params.get("type", component::command::command_t::NONE);
	std::vector<gamestate::entity_id_t> ids = params.get("entity_ids",
	                                                     std::vector<gamestate::entity_id_t>{});

	// For MOVE with multiple entities, compute each unit's current position
	// relative to the group centroid and issue FormationMoveCommand so the
	// formation is preserved at the destination.
	if ((command_type == component::command::command_t::MOVE
	     || command_type == component::command::command_t::FORMATION_MOVE)
	    && ids.size() > 1) {
		auto centroid = params.get("target", coord::phys3{0, 0, 0});

		// Gather current positions of all entities that have a POSITION component.
		std::vector<std::pair<entity_id_t, coord::phys3>> entity_positions;
		entity_positions.reserve(ids.size());
		for (auto id : ids) {
			auto entity = gstate->get_game_entity(id);
			if (entity->has_component(component::component_t::POSITION)) {
				auto pos_comp = std::dynamic_pointer_cast<component::Position>(
					entity->get_component(component::component_t::POSITION));
				entity_positions.emplace_back(id, pos_comp->get_positions().get(time));
			}
		}

		// Compute the centroid of the current group.
		if (not entity_positions.empty()) {
			// Sum up ne/se/up components manually to compute a geometric centroid.
			// We accumulate into phys_t values and divide by count.
			using phys_t = coord::phys_t;
			phys_t sum_ne{0};
			phys_t sum_se{0};
			phys_t sum_up{0};
			for (const auto &[id, pos] : entity_positions) {
				sum_ne = sum_ne + pos.ne;
				sum_se = sum_se + pos.se;
				sum_up = sum_up + pos.up;
			}
			auto n = static_cast<double>(entity_positions.size());
			coord::phys3 group_center{
				phys_t{static_cast<double>(sum_ne) / n},
				phys_t{static_cast<double>(sum_se) / n},
				phys_t{static_cast<double>(sum_up) / n},
			};

			// Issue a FormationMoveCommand to each entity with its current
			// offset from the group centre.
			for (const auto &[id, pos] : entity_positions) {
				auto entity = gstate->get_game_entity(id);
				auto command_queue = std::dynamic_pointer_cast<component::CommandQueue>(
					entity->get_component(component::component_t::COMMANDQUEUE));
				coord::phys3_delta offset = pos - group_center;
				command_queue->add_command(
					time,
					std::make_shared<component::command::FormationMoveCommand>(
						centroid, offset));
			}
			return;
		}
	}

	for (auto id : ids) {
		auto entity = gstate->get_game_entity(id);
		auto command_queue = std::dynamic_pointer_cast<component::CommandQueue>(
			entity->get_component(component::component_t::COMMANDQUEUE));

		switch (command_type) {
		case component::command::command_t::IDLE:
			command_queue->add_command(time, std::make_shared<component::command::IdleCommand>());
			break;
		case component::command::command_t::MOVE:
			command_queue->add_command(
				time,
				std::make_shared<component::command::MoveCommand>(
					params.get("target",
			                   coord::phys3{0, 0, 0})));
			break;
		case component::command::command_t::ATTACK:
			command_queue->add_command(
				time,
				std::make_shared<component::command::AttackCommand>(
					params.get("target_entity_id",
			                   gamestate::entity_id_t{})));
			break;
		case component::command::command_t::GATHER:
			command_queue->add_command(
				time,
				std::make_shared<component::command::GatherCommand>(
					params.get("target_entity_id",
			                   gamestate::entity_id_t{})));
			break;
		case component::command::command_t::TRAIN:
			command_queue->add_command(
				time,
				std::make_shared<component::command::TrainCommand>(
					params.get("game_entity",
			                   std::string{})));
			break;
		case component::command::command_t::ATTACK_MOVE:
			command_queue->add_command(
				time,
				std::make_shared<component::command::AttackMoveCommand>(
					params.get("target",
			                   coord::phys3{0, 0, 0})));
			break;
		case component::command::command_t::PATROL:
			command_queue->add_command(
				time,
				std::make_shared<component::command::PatrolCommand>(
					params.get("waypoint_from", coord::phys3{0, 0, 0}),
					params.get("waypoint_to",   coord::phys3{0, 0, 0})));
			break;
		case component::command::command_t::GUARD:
			command_queue->add_command(
				time,
				std::make_shared<component::command::GuardCommand>(
					params.get("target_entity_id",
			                   gamestate::entity_id_t{})));
			break;
		case component::command::command_t::SET_STANCE: {
			// SET_STANCE is immediate: update the Stance component directly.
			if (entity->has_component(component::component_t::STANCE)) {
				auto stance_comp = std::dynamic_pointer_cast<component::Stance>(
					entity->get_component(component::component_t::STANCE));
				stance_comp->set_stance(time,
				                        params.get("stance",
				                                   component::stance_t::AGGRESSIVE));
			}
		} break;
		case component::command::command_t::FORMATION_MOVE: {
			// Single-entity formation move: zero offset (same as regular move).
			command_queue->add_command(
				time,
				std::make_shared<component::command::FormationMoveCommand>(
					params.get("target", coord::phys3{0, 0, 0}),
					coord::phys3_delta{0, 0, 0}));
		} break;
		default:
			break;
		}
	}
}

time::time_t SendCommandHandler::predict_invoke_time(const std::shared_ptr<openage::event::EventEntity> & /* target */,
                                                     const std::shared_ptr<openage::event::State> & /* state */,
                                                     const time::time_t &at) {
	return at;
}

} // namespace event
} // namespace openage::gamestate
