// Copyright 2026-2026 the openage authors. See copying.md for legal info.

#pragma once

#include <string>


namespace openage::gamestate::api {

/**
 * Case-insensitive substring match (ASCII).
 */
bool name_contains_ci(const std::string &haystack, const std::string &needle);

/**
 * @return true if \p fqon looks like a street/road building.
 */
bool is_street_building(const std::string &fqon);

/**
 * @return true if \p fqon looks like a bridge building.
 */
bool is_bridge_building(const std::string &fqon);

} // namespace openage::gamestate::api
