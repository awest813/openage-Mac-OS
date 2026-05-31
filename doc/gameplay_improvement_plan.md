# Gameplay Improvement Plan

This document tracks the plan for restoring and then extending gameplay in openage.
It is divided into phases, moving from foundational systems (required for any gameplay)
through quality-of-life improvements and finally new features beyond the original game.

The engine currently has: movement, idle, turn, selectable, and live (attribute/HP) components,
plus a flow-field pathfinder. The components and activity system provide the hooks to build on.

---

## Phase 1 — Core Gameplay Foundations

These are the minimum systems required for a functional, playable game loop.

### 1.1 Combat System

**Status:** ✅ Complete

The `LIVE` component already stores attributes (e.g. HP) as time-indexed curves.
What is missing is the act of dealing damage.

- [x] Add `ATTACK` command type (`command_t::ATTACK`)
- [x] Add `ATTACK` component type (`component_t::ATTACK`)
- [x] Add `ATTACK_COMMAND` / `ATTACK_DEFAULT` system IDs
- [x] Create `AttackCommand` — carries the target entity ID
- [x] Create `Attack` API component — wraps the nyan `Attack` ability (damage, range, reload time)
- [x] Create `Attack` system — resolves attack commands, applies damage via `Live::set_attribute`
- [x] Integrate `Attack` system into the activity graph (new task node / command-type branch)
- [x] Death detection: when HP reaches 0, remove the entity from the world
- [x] Range check: only deal damage when the attacker is within attack range
- [x] Animation: switch to attack animation via `render_update` while attacking

### 1.2 Resource Gathering

**Status:** ✅ Complete

- [x] Add `GATHER` command type and `GatherCommand` (carries resource entity target)
- [x] Add `GATHER` component type and API component (gather rate, resource type)
- [x] Add resource storage to `Player` (time-indexed curves per resource type)
- [x] Create `Gather` system — checks range, extracts from resource entity's Live component, credits player resources
- [x] Deplete resources: remove resource entity when exhausted
- [x] Drop-off: unit must return to a drop-off building before gathering again

### 1.3 Unit Production

**Status:** ✅ Complete

Design: training is modelled on the existing command + system pattern. A
producing entity (e.g. a building) carries a `Create` API component wrapping the
nyan `engine.ability.type.Create` ability. The ability holds a set of
`CreatableGameEntity` objects, each with a `game_entity` (the unit to spawn), a
`creation_time`, and a resource cost (`cost_resource` + `cost_amount`).

A `TRAIN` command carries the fqon of the unit to produce. The `Production`
system finds the matching creatable, verifies the owner can afford it, deducts
the cost from the player's resources, and schedules a `"game.spawn_production"`
event at the completion time. Because systems cannot reach the `EntityFactory`,
the actual spawn happens in `SpawnProductionHandler` when that event fires. The
`GameState` production-request queue (`request_production` /
`take_completed_productions`) is kept as a bounded "in-progress production" view
for queries/UI and tests, drained as units finish.

- [x] Add `TRAIN` command type and `TrainCommand` (carries the unit fqon)
- [x] Add `CREATE` component type and `Create` API component (creatables, cost, build time)
- [x] Add `TRAIN_COMMAND` system id and `next_command_train` condition
- [x] Resource cost check before queuing; deduct resources on train start
- [x] `Production` system: validate creatable, check/deduct cost, queue timed request
- [x] Timed production request queue on `GameState` (`request_production` / `take_completed_productions`)
- [x] Wire `Create` into the entity factory, activity graph, and `send_command`
- [x] Drain completed production requests: `Production::train_command` fires a `"game.spawn_production"` event at `completion_time`; `SpawnProductionHandler` (registered in the simulation) creates and adds the entity at the producer's position when the event fires
- [x] Add `BUILD` command type for placing new buildings
- [x] **Honour the build placement position** — `BuildCommand` already carries the
  player-selected `target` (`coord::phys3`). `Build::build_command` now forwards it
  to the spawn event under the `"spawn_pos"` param, and `SpawnProductionHandler`
  uses an explicit `spawn_pos` verbatim when present (BUILD) and otherwise falls
  back to the producer-derived position (TRAIN). Previously buildings appeared one
  tile from the villager instead of where the player clicked. Test:
  `build_command_placement`.
- [x] **Walk to the build site before constructing** — `Build::build_command` now
  checks the builder's distance to the target. While farther than `BUILD_RANGE`
  (2 tiles) and the builder has `MOVE`, it steps toward the site via
  `Move::move_default` and re-enqueues the `BuildCommand` (the same self-re-enqueue
  pattern as `AttackMove` / `Patrol`, wired through `build → wait_for_build →
  {idle | condition_cmd_type}`). Resources are only spent and the
  `game.spawn_production` event is only scheduled once the builder has reached the
  site, so construction (the `creation_time` wait) starts on arrival rather than
  immediately. Builders without `MOVE` construct in place as before.
- [x] **Rally points** — a producing building can be given a rally point;
  units it produces are automatically sent there on spawn. `command_t::SET_RALLY_POINT`
  is handled immediately in `SendCommandHandler` (like `SET_STANCE`) and stored on
  `GameState` (`set_/get_/has_/clear_rally_point`, keyed by entity ID, cleared in
  `remove_game_entity`). `SpawnProductionHandler` queues a `MoveCommand` to the
  rally point on the new unit (if it has `MOVE` + a command queue) before its
  activity is initialised, so it walks there immediately. Test:
  `rally_point_lifecycle`.

### 1.4 Win / Loss Conditions

**Status:** ✅ Complete

- [x] Add `player_state_t` enum (ALIVE / DEFEATED / WINNER) and `get_state()` / `set_state()` to `Player`
- [x] Defeat a player when their last building is destroyed: `GameState::remove_game_entity(id, time)` detects building death (owned entity without MOVE component) and calls `check_defeat`
- [x] `check_defeat` marks the player DEFEATED, then scans for a sole remaining alive player and marks them WINNER
- [x] Broadcast: `GameState` fires `"game.player_defeated"` and `"game.game_over"` events through the stored event loop; `PlayerDefeatedHandler` and `GameOverHandler` log the outcome (future: UI overlay, network broadcast)

### 1.5 Phase 1 Audit & Polish

**Status:** ✅ Complete

A correctness pass over all of Phase 1 fixed two leaks and several edge cases:

- [x] **Carried-resource state moved off the global static** — `gather.cpp` used a
  file-scope `std::unordered_map` + mutex for carried cargo, which leaked when a
  carrying gatherer died and broke determinism across simulations. Moved into
  `GameState` (`is_/get_/set_/clear_carried_resource`) and cleared in
  `remove_game_entity`.
- [x] **Production queue no longer grows unbounded** — `production_requests` was
  appended on every train but never drained in the real flow (the spawn event
  did the work). `SpawnProductionHandler` now drains completed requests, keeping
  the queue as an accurate bounded "in-progress production" view.
- [x] **Graceful unknown-owner handling** — gather drop-off and `Production` now
  guard with `GameState::has_player` instead of letting `get_player` throw out of
  system dispatch.
- [x] **Sole-survivor / mutual defeat** — `check_defeat` now fires `game.game_over`
  for `alive_count <= 1` (with a `has_winner` flag) instead of only the exactly-one
  case.
- [x] Documented the building heuristic (`OWNERSHIP` + no `MOVE`) as a TODO pending
  a real unit/building type system, and the `make_shared` requirement on `GameState`.
- [x] New regression tests: `carried_resources_lifecycle`; defeat tests register the
  game-over handlers (these `create_event` paths would otherwise throw).

### 1.6 Population Cap

**Status:** ✅ Core complete (data-sourced costs/capacity pending)

Population is modelled on `Player` with two time-indexed `Discrete<int64_t>`
curves — `population_demand` (space consumed by living + in-production units)
and `population_capacity` (headroom from buildings) — mirroring the resource
model so it is deterministic and rewindable.

- [x] `Player` population API: `init_population`, `get_population_demand`,
  `get_population_capacity` (clamped to `POPULATION_MAX = 200`),
  `get_population_space`, `has_population_space`, `add_population_demand`,
  `add_population_capacity`. Demand and capacity are never recorded below 0.
- [x] **Training gate** — `Production::train_command` blocks training (without
  spending resources) when `has_population_space` is false, and reserves
  `DEFAULT_POPULATION_COST` on a successful train so queued units count against
  the cap immediately.
- [x] **Release on death** — `GameState::remove_game_entity(id, time)` releases the
  reserved population when an owned unit (has `MOVE`) dies.
- [x] New unit test: `player_population` (demand/capacity/space, the gate, the
  `POPULATION_MAX` clamp, and time-indexed history).
- [ ] **Per-unit population cost from nyan** — currently every unit costs
  `DEFAULT_POPULATION_COST = 1`; should come from a nyan `PopulationSpace`
  ability once the API/data is wired (same constraint as the unit/building
  heuristic).
- [x] **Building-provided capacity** — a completed building raises its owner's
  `population_capacity` by `DEFAULT_BUILDING_POPULATION_SPACE` (`SpawnProductionHandler`)
  and `GameState::remove_game_entity` lowers it again on destruction, using the
  same `OWNERSHIP` + no-`MOVE` building heuristic. So houses/town centres now lift
  the cap and losing them lowers it. Test: `building_population_capacity`.
  *Limitation:* until nyan `PopulationSpace` data is wired, **every** building
  contributes the same default (not just houses/TCs), and by a fixed amount.
  Pre-placed starting units/buildings must still register their demand/capacity at
  game setup via the `Player` API.

---

## Phase 2 — Quality-of-Life Improvements

These improve the experience once Phase 1 is functional.

### 2.1 Unit Behaviour

**Status:** ✅ Complete

Design: all behaviours follow the existing command+system+activity pattern. A new `Stance`
internal component (`component_t::STANCE`, `curve::Discrete<stance_t>`) stores the unit's
combat stance over time; default is AGGRESSIVE. The `Idle` system was extended to accept
`GameState` and performs an auto-attack scan when the stance permits it.

- [x] **Attack stances** — `stance_t` enum (AGGRESSIVE / DEFENSIVE / STAND_GROUND / NO_ATTACK).
  `Stance` internal component holds a `curve::Discrete<stance_t>`; all entities get one (default
  AGGRESSIVE) in `init_components`. `SET_STANCE` command handled directly in `SendCommandHandler`.
- [x] **Auto-attack** — `Idle::idle` now receives `GameState`; scans for enemies in `max_range × 5`
  (AGGRESSIVE) or `max_range × 1` (DEFENSIVE / STAND_GROUND) and pushes an `AttackCommand` if one
  is found. STAND_GROUND and NO_ATTACK are respected.
- [x] **Attack-move command** — `command_t::ATTACK_MOVE` / `AttackMoveCommand(coord::phys3)` /
  `AttackMove::attack_move_command` system. Scans for enemies in attack range on each tick;
  if found pushes `AttackCommand + AttackMoveCommand` (self-re-enqueue); otherwise calls
  `Move::move_default` and re-enqueues. Activity wires `wait_for_attack_move` the same way as
  move/gather.
- [x] **Patrol** — `command_t::PATROL` / `PatrolCommand(from, to)` / `Patrol::patrol_command`.
  Moves toward `waypoint_to`, swaps waypoints on arrival, scans for enemies on each leg;
  always self-re-enqueues (until an explicit new command interrupts via the XorEventGate).
- [x] **Guard / follow** — `command_t::GUARD` / `GuardCommand(entity_id_t)` / `Guard::guard_command`.
  Follows the target if farther than `GUARD_RADIUS = 2.0`; otherwise scans for enemies in attack
  range and attacks them. Polls at `GUARD_SCAN_INTERVAL = 0.5 s` when idle. Guard ends naturally
  when the target entity is destroyed.
- [x] New unit tests: `stance_component`, `next_command_conditions_extended`

### 2.2 Pathfinding Improvements

- ✅ Collision avoidance: tile occupancy tracking in `GameState`; `Move::move_default` checks occupancy and retries when the destination tile is blocked
- ✅ Formation movement: move a group while preserving relative positions
- ✅ Hazard cost modifier: tiles within enemy `Attack.max_range` get +20 path cost before each `Move` path query (`GameState::apply_hazard_path_costs`, `Map::restore_sector_costs` baseline snapshots)
- ✅ Blocked-path re-routing: if the path becomes blocked, recalculate

### 2.3 Interface Improvements

*(see also `doc/ideas/interface.md`)*

- [ ] Select more than 30 units (engine already removes the cap, UI must follow)
- [ ] Customisable hotkeys for all actions
- [ ] Minimap improvements: symbols for key buildings, big-map overlay
- [ ] After-game statistics screen (kills, resources gathered, APM)
- [ ] Zoom towards mouse cursor / selected units

### 2.4 Fog of War

- ✅ Track which tiles each player has explored (`FogOfWar::explored_tiles`)
- ✅ Track which tiles are currently visible (line-of-sight per unit) (`FogOfWar::visible_tiles`, `update_visibility`)
- ✅ Rebuild visibility each simulation tick (`GameState::refresh_visibility`, called from `GameSimulation::run`)
- ✅ Hide enemy units outside visible tiles (`GameState::update_fog_render_visibility` sets `fog_display_t::HIDDEN` on render entities; `WorldRenderStage` skips hidden objects)
- ✅ Show last-known position of units that leave vision (`fog_display_t::GHOST` draws at `FogOfWar::last_known_positions` via `WorldObject::fetch_updates`)
- ✅ Terrain fog overlay: per-tile fog texture rebuilt each tick (`FogTileTexture`, `GameState::update_fog_tile_texture`); terrain shader darkens explored tiles and blacks out unexplored areas (`TerrainRenderStage::update_fog_overlay`)
- [ ] Minimap fog overlay (HUD minimap not implemented yet)
- ✅ Ghost unit visuals: last-known units render desaturated and semi-transparent (`fog_ghost` uniform in `world2d.frag.glsl`)
- ✅ **Ghost recording fix** — `GameState::is_entity_visible` now records an entity's
  position whenever it *is* visible (the spot it was last seen) and leaves that
  entry untouched once it goes out of vision, so it renders as a `GHOST`.
  Previously a last-known position was only stored if the entity's *new, hidden*
  tile happened to be explored, so a unit moving into unexplored fog incorrectly
  showed as `HIDDEN`. Fixed the `fog_render_visibility` test (was failing).

---

## Phase 3 — Extended Features

New features beyond the original Age of Empires gameplay.
All of these must remain opt-in; a "vanilla mode" is always available.

### 3.1 Environment

*(see `doc/ideas/gameplay.md` — Environment section)*

- [ ] Day/night cycle affecting line-of-sight
- [ ] Weather effects (fog, rain) modifying movement speed and visibility
- [ ] Forest hiding: a unit in a forest tile is invisible to enemies beyond a threshold

### 3.2 New Resources and Economy

- [x] Resource salvage piles from destroyed buildings — when a building with a
  recorded construction cost is destroyed, a neutral salvage pile spawns at the
  site (50% of cost by default). Villagers can gather it; the pile decays at
  1 resource per 10 s. Cost is recorded when a building is placed via BUILD.
- [ ] Building deconstruction to recover materials
- [ ] Infinite forest regeneration (configurable)

### 3.3 New Buildings

- [ ] Bridges: buildable over water, block ships, allow land units
- [ ] Streets: increase movement speed for units travelling over them

### 3.4 AI Improvements

*(see `doc/ideas/ai.md`)*

- [ ] Python AI hook API: `on_start`, `on_frame`, `on_end`, unit-level events
- [ ] Villager pathfinding awareness of hazardous areas
- [ ] Earlier raiding AI: smaller attack waves in feudal age instead of one large lump

### 3.5 Multiplayer Enhancements

- [ ] Spectator / casting mode with free camera and picture-in-picture
- [ ] Tournament container format (settings, replays, civ draft)
- [ ] In-game voice communication between teammates
- [ ] Skill-based matchmaking via the openage master server

---

## Build & Test Environment

The full suite builds and runs green on Ubuntu 24.04 (matching CI's devenv):

```
# system deps: see packaging/docker/devenv/Dockerfile.ubuntu.2404
python3.12 -m pip install "cython>=3.0.10,<4.0.0" --break-system-packages
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release -DPython3_EXECUTABLE=/usr/bin/python3.12 \
      -DDOWNLOAD_NYAN=YES -G Ninja ..
cmake --build . --parallel "$(nproc)"
./run test -a          # all 58 tests pass (exit 0)
```

Notes:
- Ubuntu 24.04's default `python3` may be a 3.11 that lacks numpy/mako; point
  CMake at 3.12 with `-DPython3_EXECUTABLE=/usr/bin/python3.12`. Cython must be
  installed for *that* interpreter.
- `Map` construction is robust to a database without the `engine.util.path_type.PathType`
  base object (it builds with no pathfinding grids) so gamestate unit tests can
  construct a `Map` without loading the full nyan API. This fixed an abort in the
  `fog_tile_texture` test that previously took down the whole `./run test -a` run.

## Implementation Notes

- All new commands follow the pattern in
  `libopenage/gamestate/component/internal/commands/` (see `move.h` / `move.cpp`).
- All new API components follow the pattern in
  `libopenage/gamestate/component/api/` (see `move.h`, `live.h`).
- All new systems follow the pattern in
  `libopenage/gamestate/system/` (see `move.h` / `move.cpp`).
- New source files must be added to the relevant `CMakeLists.txt`.
- Damage values, ranges, and rates are stored in **nyan** data objects, not hard-coded.
