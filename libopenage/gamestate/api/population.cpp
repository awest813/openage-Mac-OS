// Copyright 2026-2026 the openage authors. See copying.md for legal info.

#include "population.h"

#include <string_view>


namespace openage::gamestate::api {

namespace {

constexpr std::string_view POPULATION_SPACE_SUFFIX = "PopulationSpace";

bool is_population_space_resource(const std::string &resource_fqon) {
	if (resource_fqon.size() < POPULATION_SPACE_SUFFIX.size()) {
		return false;
	}
	return resource_fqon.compare(resource_fqon.size() - POPULATION_SPACE_SUFFIX.size(),
	                             POPULATION_SPACE_SUFFIX.size(),
	                             POPULATION_SPACE_SUFFIX)
	       == 0;
}

int64_t sum_population_amounts(const std::shared_ptr<nyan::View> &db_view,
                               const nyan::set_t &amounts) {
	int64_t total = 0;

	for (const auto &amount_val : amounts) {
		auto amount_fqon = std::dynamic_pointer_cast<nyan::ObjectValue>(amount_val.get_ptr())->get_name();
		auto amount_obj = db_view->get_object(amount_fqon);

		auto resource_fqon = std::string{
			amount_obj.get<nyan::ObjectValue>("ResourceAmount.type")->get_name()};
		if (not is_population_space_resource(resource_fqon)) {
			continue;
		}

		auto amount = amount_obj.get<nyan::Int>("ResourceAmount.amount")->get();
		if (amount > 0) {
			total += amount;
		}
	}

	return total;
}

int64_t lookup_population_from_ability(const std::shared_ptr<nyan::View> &db_view,
                                       const std::string &game_entity,
                                       const char *ability_parent,
                                       const char *amount_member) {
	try {
		auto game_entity_obj = db_view->get_object(game_entity);
		auto abilities = game_entity_obj.get_set("GameEntity.abilities");

		for (const auto &ability_val : abilities) {
			auto ability_fqon = std::dynamic_pointer_cast<nyan::ObjectValue>(
				ability_val.get_ptr())->get_name();
			auto ability_obj = db_view->get_object(ability_fqon);

			if (ability_obj.get_parents()[0] != ability_parent) {
				continue;
			}

			auto amounts = ability_obj.get_set(amount_member);
			return sum_population_amounts(db_view, amounts);
		}
	}
	catch (...) {
	}

	return 0;
}

} // namespace

int64_t lookup_population_demand(const std::shared_ptr<nyan::View> &db_view,
                                 const std::string &game_entity) {
	return lookup_population_from_ability(db_view,
	                                      game_entity,
	                                      "engine.ability.type.UseContingent",
	                                      "UseContingent.amount");
}

int64_t lookup_population_provision(const std::shared_ptr<nyan::View> &db_view,
                                    const std::string &game_entity) {
	return lookup_population_from_ability(db_view,
	                                      game_entity,
	                                      "engine.ability.type.ProvideContingent",
	                                      "ProvideContingent.amount");
}

} // namespace openage::gamestate::api
