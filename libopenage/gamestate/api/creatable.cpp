// Copyright 2026-2026 the openage authors. See copying.md for legal info.

#include "creatable.h"

#include <optional>

#include "gamestate/definitions.h"
#include "gamestate/game_state.h"


namespace openage::gamestate::api {

namespace {

std::optional<double> optional_float_member(const nyan::Object &obj, const char *member) {
	try {
		return obj.get<nyan::Float>(member)->get();
	}
	catch (...) {
		return std::nullopt;
	}
}

} // namespace

CreatableCostInfo lookup_creatable(const std::shared_ptr<nyan::View> &db_view,
                                   const nyan::Object &create_ability,
                                   const std::string &target_game_entity) {
	CreatableCostInfo info{};
	auto creatables = create_ability.get_set("Create.creatables");

	for (const auto &creatable_val : creatables) {
		auto creatable_fqon = std::dynamic_pointer_cast<nyan::ObjectValue>(
			creatable_val.get_ptr())->get_name();
		auto creatable_obj = db_view->get_object(creatable_fqon);

		auto game_entity = creatable_obj.get<nyan::ObjectValue>("CreatableGameEntity.game_entity");
		if (game_entity->get_name() != target_game_entity) {
			continue;
		}

		info.found = true;
		info.game_entity = target_game_entity;
		info.creation_time = creatable_obj.get<nyan::Float>("CreatableGameEntity.creation_time")->get();
		info.cost_resource = std::string{
			creatable_obj.get<nyan::ObjectValue>("CreatableGameEntity.cost_resource")->get_name()};
		info.cost_amount = creatable_obj.get<nyan::Int>("CreatableGameEntity.cost_amount")->get();

		if (auto salvage_frac = optional_float_member(creatable_obj,
		                                              "CreatableGameEntity.salvage_recovery_fraction")) {
			info.salvage_recovery_fraction = *salvage_frac;
		}
		if (auto decon_frac = optional_float_member(
			    creatable_obj, "CreatableGameEntity.deconstruct_recovery_fraction")) {
			info.deconstruct_recovery_fraction = *decon_frac;
		}
		if (auto decon_time = optional_float_member(creatable_obj,
		                                            "CreatableGameEntity.deconstruct_time")) {
			info.deconstruct_time = *decon_time;
		}

		break;
	}

	return info;
}

BuildingCostRecord building_cost_from_creatable(const CreatableCostInfo &info) {
	BuildingCostRecord record;
	record.resource_type = info.cost_resource;
	record.amount = info.cost_amount;
	record.destroy_recovery_fraction = info.salvage_recovery_fraction;
	record.deconstruct_recovery_fraction = info.deconstruct_recovery_fraction;
	if (info.deconstruct_time > 0) {
		record.deconstruct_time = info.deconstruct_time;
	}
	else if (info.creation_time > 0) {
		record.deconstruct_time = info.creation_time * DECONSTRUCT_TIME_FACTOR;
	}
	else {
		record.deconstruct_time = DECONSTRUCT_TIME_FACTOR;
	}
	return record;
}

} // namespace openage::gamestate::api
