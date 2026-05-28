// Copyright 2025-2025 the openage authors. See copying.md for legal info.

#include "../testing/testing.h"

#include <memory>

#include <nyan/nyan.h>

#include "event/event_loop.h"
#include "player.h"
#include "time/time.h"


namespace openage::gamestate::tests {

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

} // namespace openage::gamestate::tests
