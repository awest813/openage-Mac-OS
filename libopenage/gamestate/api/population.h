// Copyright 2026-2026 the openage authors. See copying.md for legal info.

#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include <nyan/nyan.h>

namespace openage::gamestate::api {

/**
 * Sum PopulationSpace amounts from a UseContingent ability on \p game_entity.
 *
 * Returns 0 when the entity is missing or has no matching contingent.
 */
int64_t lookup_population_demand(const std::shared_ptr<nyan::View> &db_view,
                                 const std::string &game_entity);

/**
 * Sum PopulationSpace amounts from a ProvideContingent ability on \p game_entity.
 *
 * Returns 0 when the entity is missing or has no matching contingent.
 */
int64_t lookup_population_provision(const std::shared_ptr<nyan::View> &db_view,
                                    const std::string &game_entity);

} // namespace openage::gamestate::api
