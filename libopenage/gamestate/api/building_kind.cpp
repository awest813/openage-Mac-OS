// Copyright 2026-2026 the openage authors. See copying.md for legal info.

#include "building_kind.h"

#include <cctype>


namespace openage::gamestate::api {

bool name_contains_ci(const std::string &haystack, const std::string &needle) {
	if (needle.empty()) {
		return true;
	}
	if (haystack.size() < needle.size()) {
		return false;
	}

	auto lower = [](unsigned char c) {
		return static_cast<char>(std::tolower(c));
	};

	for (size_t i = 0; i + needle.size() <= haystack.size(); ++i) {
		bool match = true;
		for (size_t j = 0; j < needle.size(); ++j) {
			if (lower(static_cast<unsigned char>(haystack[i + j]))
			    != lower(static_cast<unsigned char>(needle[j]))) {
				match = false;
				break;
			}
		}
		if (match) {
			return true;
		}
	}
	return false;
}

bool is_street_building(const std::string &fqon) {
	return name_contains_ci(fqon, "street") or name_contains_ci(fqon, "road");
}

bool is_bridge_building(const std::string &fqon) {
	return name_contains_ci(fqon, "bridge");
}

} // namespace openage::gamestate::api
