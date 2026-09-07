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
- [x] **Per-unit population cost from nyan** — `Production::train_command` reads
  `UseContingent` PopulationSpace amounts via `api::lookup_population_demand`;
  units without the ability cost 0. Reserved demand is passed through the spawn
  event and recorded on the entity for correct release on death. Test:
  `entity_population_tracking`, `population_lookup_missing_entity`.
- [x] **Building-provided capacity** — `SpawnProductionHandler` reads
  `ProvideContingent` PopulationSpace amounts via `api::lookup_population_provision`
  and `GameState::remove_game_entity` lowers the recorded amount on destruction.
  Buildings without ProvideContingent no longer add a blanket default. Test:
  `building_population_capacity`, `entity_population_tracking`.
  *Note:* pre-placed starting units/buildings must still register demand/capacity
  at game setup via the `Player` API when they bypass the spawn handler.

### 1.7 Building and Unit Repair

**Status:** ✅ Complete

Villagers and builders can repair damaged friendly buildings, siege weapons, and
ships over time, restoring their hit points and deducting proportional resources,
matching the Age of Empires II mechanic documented in
`doc/reverse_engineering/game_mechanics/repair.md`.

- [x] Add `REPAIR` command type (`command_t::REPAIR`) and `RepairCommand` (carrying target entity ID)
- [x] Add `REPAIR_COMMAND` system ID and `next_command_repair` activity condition
- [x] Entity max HP tracking on `GameState` (`set_entity_max_hp` / `get_entity_max_hp`), cleaned up on entity removal
- [x] Create `Repair` system — verifies target exists, has `LIVE` and `POSITION`, and is friendly
- [x] Range check and approach — if builder is farther than `REPAIR_INTERACTION_RANGE` (2 tiles) and has `MOVE`, steps toward target and re-enqueues `RepairCommand`
- [x] Speed differentiation — buildings repair at `BUILDING_REPAIR_HP_PER_SEC` (12.5 HP/s = 750 HP/min); units/siege/ships repair at `UNIT_REPAIR_HP_PER_SEC` (3.133 HP/s = 188 HP/min)
- [x] Cost deduction — `0.5 * (build_cost) / (max_hp) * (repaired_hp)` deducted from player resources; pauses if player cannot afford
- [x] Integrated into entity factory activity graph and `SendCommandHandler`
- [x] Tests: `repair_command_lifecycle`, `repair_approach_movement`, `entity_max_hp_tracking`

### 1.8 Garrison and Town Bell System

**Status:** ✅ Complete

Units can garrison inside friendly buildings (Town Centers, Towers, Castles) and
vehicles (Rams, Transports) for protection, healing, and projectile damage boosting,
matching the Age of Empires II mechanic documented in
`doc/reverse_engineering/game_mechanics/garrison.md` and
`doc/reverse_engineering/game_mechanics/town_bell.md`.

- [x] Add `GARRISON` and `UNGARRISON` command types (`command_t::GARRISON`, `command_t::UNGARRISON`)
- [x] Create `GarrisonCommand(target_entity)` and `UngarrisonCommand()`
- [x] Add `GARRISON_COMMAND` and `UNGARRISON_COMMAND` system IDs
- [x] Add `next_command_garrison` and `next_command_ungarrison` activity conditions
- [x] Building garrison container tracking on `GameState` (`building_garrisons`, `unit_garrison_parent`)
- [x] Garrison capacities per entity type (`GARRISON_CAPACITY_TOWN_CENTER = 15`, `GARRISON_CAPACITY_CASTLE = 20`, `GARRISON_CAPACITY_TOWER = 5`, `GARRISON_CAPACITY_RAM = 4`), with custom overrides via `set_garrison_capacity`
- [x] Range check and approach movement in `Garrison::garrison_command` via `Move::move_default` when unit is farther than `GARRISON_INTERACTION_RANGE` (2.0 tiles)
- [x] Tile occupancy release and position locking while garrisoned
- [x] Safe ejection / ungarrisoning in `GameState::ungarrison_entities`: placing units at rally point or adjacent tile, restoring tile occupancy, and re-enqueuing saved tasks
- [x] Projectile boost formula: `additional_arrows = floor(sum(unit_dps_pierce) / building_dps)` applied in `Attack::attack_default`
- [x] Town Bell and Back to Work: `ring_town_bell(tc_id)` scans workers within 25 tiles, saves current tasks, and routes to nearest garrison; `back_to_work(tc_id)` ungarrisons workers and restores their prior tasks
- [x] Passive garrison healing: `tick_garrison_heal` restores `GARRISON_HEAL_HP_PER_SEC` (0.1 HP/s) to biological units
- [x] Automatic ejection on building destruction in `GameState::remove_game_entity`
- [x] Tests: `garrison_lifecycle`, `garrison_capacity_and_ownership`, `garrison_arrow_bonus_and_damage`, `town_bell_and_back_to_work`, `garrison_passive_heal`

### 1.9 Advanced Combat & Damage Parity

**Status:** ✅ Complete

Complete Age of Empires II damage calculation, armor class resistance matrix, elevation modifiers,
minimum range enforcement, and siege splash/area damage conforming to `doc/reverse_engineering/game_mechanics/damage.md`.

- [x] Defined `armor_class_t` enum with 15 AoE2 canonical armor classes (MELEE, PIERCE, INFANTRY, SPEARMAN, CAVALRY, ARCHER, SIEGE_WEAPON, BUILDING, WALL_GATE, SHIP, RAM, WAR_ELEPHANT, EAGLE_WARRIOR, MONK, CAMEL)
- [x] Default unpossessed armor set to `DEFAULT_UNPOSSESSED_ARMOR = 1000` to prevent unintended bonus damage leakage
- [x] Curve-backed getters/setters on `component::Attack` (`get_damage`, `set_damage`, `get_reload_time`, `set_reload_time`, `get_max_range`, `set_max_range`, `get_min_range`, `set_min_range`, `get_blast_radius`, `set_blast_radius`, `get_attack_type`, `set_attack_type`) with dynamic nyan query fallbacks
- [x] GameState combat records: `set_entity_armor`, `get_entity_armor`, `has_entity_armor`, `set_entity_attack_bonus`, `get_entity_attack_bonus`, `get_entity_attack_bonuses`, with automatic cleanup on entity removal
- [x] Minimum range check: aborts attack and returns 0 damage if `dist < min_range`
- [x] Primary armor mitigation: `max(0, base_damage - primary_armor)` for MELEE and PIERCE attacks
- [x] Class bonus summation: `sum(max(0, attack_bonus - target_class_armor))` for all bonuses
- [x] Elevation scaling: `ELEVATION_DAMAGE_BONUS = 1.25` (+25% downhill) and `ELEVATION_DAMAGE_MALUS = 0.75` (-25% uphill) for elevation differential $\ge 0.25$
- [x] Guaranteed minimum 1 damage floor on all connecting attacks
- [x] Siege blast radius & area damage: secondary targets within `blast_radius` take distance-falloff splash damage `damage * (1 - dist / (2 * radius))`
- [x] Tests: `combat_melee_and_pierce_armor`, `combat_attack_bonuses`, `combat_elevation_modifier`, `combat_minimum_range`, `combat_siege_splash_damage`

### 1.10 Market Economy, Trading & Tribute System

**Status:** ✅ Complete

Complete Age of Empires II market trading (buying/selling commodities), dynamic price fluctuation,
tribute transfers with tax rates, and Trade Cart / Cog continuous distance-based gold routes conforming
to `doc/reverse_engineering/game_mechanics/market.md` and `doc/reverse_engineering/networking/12-market.md`.

- [x] Defined `market_resource_t` enum with 4 AoE2 resources (FOOD, WOOD, STONE, GOLD)
- [x] Global base pricing: initial Wood 100, Food 100, Stone 130
- [x] Standard market transaction fee `MARKET_DEFAULT_FEE = 0.30` (buy price $= \lfloor \text{base} \times 1.30 \rfloor$, sell price $= \lfloor \text{base} \times 0.70 \rfloor$)
- [x] Per-player fee customization (`set_market_fee`) to support Guilds tech (15%) and Saracen civ bonus (5%)
- [x] Dynamic price shifts: buying increments base price by 3 gold per 100 units; selling decrements base price by 3 gold per 100 units
- [x] Hard bounds: base price clamped within `[MARKET_MIN_BASE_PRICE = 20, MARKET_MAX_BASE_PRICE = 9999]`
- [x] Tribute system on `GameState` (`send_tribute`): sender pays `amount + floor(amount * fee)`, recipient receives `amount`
- [x] Configurable tribute fee (`set_tribute_fee`) to support Coinage (20%) and Banking (10%)
- [x] After-game statistics on `Player`: time-indexed curves for `tribute_sent`, `tribute_received`, and `trade_profit`
- [x] Added `command_t::TRADE` and `TradeCommand(target_market, home_market, state)`
- [x] Added `system_id_t::TRADE_COMMAND` and `Trade::trade_command` system
- [x] Distance gold formula: canonical Conquerors formula $2.0 \times (d/\text{size} + 0.3) \times d + 0.5$ based on Euclidean distance between markets
- [x] Continuous trade loop: approaches target market $\rightarrow$ loads gold payload into `CarriedResource` $\rightarrow$ returns home $\rightarrow$ deposits gold to player stockpile and records profit $\rightarrow$ reverses back to target
- [x] Market destruction edge cases: if target market is destroyed, trade cart with cargo returns home to deposit and stops; if home market is destroyed, auto-rebinding to closest alternative friendly market
- [x] Building detection: `is_market_building` and `is_dock_building` helpers in `building_kind.h`
- [x] Tests: `market_commodity_buy_and_sell`, `market_dynamic_price_fluctuations`, `market_fee_customization`, `market_tribute_system_and_fees`, `trade_cart_gold_distance_formula`, `trade_cart_continuous_trading_loop_and_market_destruction`

### 1.11 Technology Trees, Research Queues & Age Progression

**Status:** ✅ Complete

Complete Age of Empires II research and technology tree framework, age progression (Dark Age $\rightarrow$ Feudal Age $\rightarrow$ Castle Age $\rightarrow$ Imperial Age), building research queues, duplicate-research locks, cancellation refunds, and immediate effect callbacks. Conforms to `doc/reverse_engineering/game_mechanics/tech_tree.md` and `doc/reverse_engineering/networking/technology_ids.md`.

- [x] Defined `age_t` enum with 4 AoE2 ages (`DARK_AGE`, `FEUDAL_AGE`, `CASTLE_AGE`, `IMPERIAL_AGE`)
- [x] Time-indexed `current_age` curve (`Discrete<int64_t>`) on `Player` with historical time queries (`get_age(time)`, `set_age(time, age)`)
- [x] Researched technology set on `Player` (`has_researched`, `mark_researched`, `get_tech_count`)
- [x] Canonical technology definitions (`TechDefinition` catalog in `GameState`): standard tech IDs, age requirements, research times, resource costs, and effect callbacks
- [x] Default technology catalog: Feudal Age (101), Castle Age (102), Imperial Age (103), Loom (22), Coinage (23), Banking (17), Guilds (15), Forging (67)
- [x] Active research tracking on `GameState` (`building_research`, `ActiveResearch`), per-player lock to prevent duplicate concurrent research across multiple buildings (`active_techs_in_progress`)
- [x] 100% cost refund on manual cancellation (`cancel_research`)
- [x] Building destruction handling: research cancelled, lock released so player can research at another building, but resources lost (AoE2 parity)
- [x] Dynamic effect dispatch: tech completion invokes callback (e.g. `set_age`, adjusting tribute fees via Coinage/Banking, adjusting market fee via Guilds)
- [x] Added `command_t::RESEARCH` and `ResearchCommand(tech_id)`
- [x] Added `ResearchCompleteHandler` for `"game.complete_research"` and `GameState::tick_research` integrated into simulation loop
- [x] Unit tests: `player_age_progression_lifecycle`, `research_queue_cost_and_prerequisites`, `research_cancellation_and_full_refund`, `research_completion_and_age_advancement`, `technology_effect_application`, `research_building_destruction_handling`

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

- [x] **Select more than 30 units** — `DragSelectHandler` has no selection cap; every
  owned selectable entity in the drag rectangle is included. The legacy 30-unit limit
  is not enforced in the engine. QML `ActionMode` wiring (when present) should mirror
  this behaviour.
- [ ] **Customisable hotkeys for all actions** — `cfg/keybinds.oac` is the editable
  binding profile (loaded at start via the cvar system). The new Qt input stack still
  hard-codes camera/game binds; full keybinds.oac → `InputContext` mapping is pending.
- [x] **Minimap improvements (data layer)** — `GameState::update_minimap_texture`
  builds an RGBA minimap each visibility tick: fog-based terrain shading plus markers
  (larger symbols for buildings, dots for units, colour by ownership). Test:
  `minimap_texture`. HUD texture display and big-map overlay still pending QML wiring.
- [x] **After-game statistics (data layer)** — `Player` accumulates per-game
  stats as time-indexed curves: `units_killed` (credited to the attacker's owner
  in `Attack::attack_default` on a kill), `units_lost` (recorded in
  `GameState::remove_game_entity(id, time)` for any owned unit/building death),
  and `resources_gathered` per resource type (recorded on drop-off in the Gather
  system). Query via `get_units_killed` / `get_units_lost` /
  `get_resource_gathered` / `get_total_resources_gathered`. APM is tracked via
  `record_action` (counted once per player-issued command in `SendCommandHandler`;
  internal re-enqueues bypass it) and queried with `get_actions_issued` /
  `get_apm`. Test: `player_statistics`.
- [x] **After-game summary + menu shell (QML)** — Presenter loads
  `assets/qml/menus/main.qml` instead of the test GUI. `MenuController` bridges
  main / pause / game-over screens to the simulation clock and window. Escape
  toggles pause (and returns from game-over to the main menu); match end stores
  `GameState::GameResult` (mutex-guarded) and opens the summary with kills /
  losses / resources / APM. Menu screens block game/camera input; texture /
  animation / terrain placeholders are wired so missing assets fall back instead
  of crashing. The time loop starts paused until **Start**. Full match reset from
  the menu is still pending (Start re-enters the current simulation).
- [x] **Zoom towards mouse cursor** — wheel zoom uses `Camera::zoom_towards` anchored
  on the cursor by default (`CameraManager::ZoomAnchor::MOUSE_CURSOR`). Set
  `CAMERA_ZOOM_ANCHOR screen_center` in `cfg/camera.oac` for legacy centre zoom.
  Zoom towards selected units is still pending.

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

**Status:** ✅ Complete (opt-in; vanilla defaults preserved)

- [x] **Day/night cycle affecting line-of-sight** — opt-in via `GAMEPLAY_DAY_NIGHT`
  (`cfg/gameplay.oac`, default off). `GameState` resolves `day_phase_t`
  (DAY / DUSK / NIGHT / DAWN) from simulation time and applies sight multipliers
  (`DAY_SIGHT_MULT` / `TWILIGHT_SIGHT_MULT` / `NIGHT_SIGHT_MULT`) in
  `refresh_visibility`. Day/night lengths configurable via
  `set_day_night_params`. Test: `environment_day_night`.
- [x] **Weather effects (fog, rain) modifying movement speed and visibility** —
  opt-in via `GAMEPLAY_WEATHER`. Cycles CLEAR → FOG → RAIN on
  `tick_environment` (each simulation tick). Fog and rain reduce sight
  (stacked with day/night); rain also applies `WEATHER_RAIN_MOVE_MULT` through
  `GameState::get_move_speed_multiplier` used by `Move::move_default`. Test:
  `environment_weather`.
- [x] **Forest hiding** — opt-in via `GAMEPLAY_FOREST_HIDE`. Enemy units on
  forest tiles are invisible beyond `FOREST_HIDE_THRESHOLD_TILES` (Chebyshev,
  default 2) even when fog-visible. Forest tiles are marked via
  `mark_forest_tile` or `rebuild_forest_tiles_from_terrain` (matches "forest"
  in terrain asset path / fqon). Idle auto-attack respects
  `is_entity_visible`. Test: `environment_forest_hide`.

### 3.2 New Resources and Economy

- [x] Resource salvage piles from destroyed buildings — when a building with a
  recorded construction cost is destroyed, neutral salvage pile(s) spawn at the
  site (50% of cost by default, overridable per creatable via nyan). Multi-resource
  costs spawn one pile per resource type. Villagers can gather them; each pile
  decays at 1 resource per 10 s. Cost is recorded when a building is placed via
  BUILD or trained from a Create producer (parsed from nyan `ResourceCost`).
- [x] Building deconstruction to recover materials — `DECONSTRUCT` command: villager
  walks to an owned building, waits `deconstruct_time`, then spawns salvage at
  `deconstruct_recovery_fraction` (default 75%, per-creatable in nyan) and removes
  the building without combat destroy-salvage. Salvage still spawns if the building
  is destroyed by combat before the timer ends (cost snapshot is in the event).
- [x] **Infinite forest regeneration (configurable)** — opt-in via the
  `GAMEPLAY_FOREST_REGEN` cvar (`cfg/gameplay.oac`, default off → vanilla). When
  enabled, the Gather system registers each harvestable resource node it taps
  (recording the amount as the regen ceiling) and a depleted node is kept in the
  world at 0 instead of being removed. `GameState::tick_resource_regen` (called
  each simulation tick alongside salvage decay) restores `FOREST_REGEN_AMOUNT`
  units every `FOREST_REGEN_INTERVAL_SEC` toward the ceiling. Registrations are
  cleaned up when a node is removed or loses its `LIVE` component. Config knobs:
  `GameState::set_forest_regen_enabled` / `set_forest_regen_params`. Test:
  `resource_node_regen`.

### 3.3 New Buildings

**Status:** ✅ Complete (opt-in; vanilla defaults preserved)

- [x] **Streets** — opt-in via `GAMEPLAY_STREETS` (`cfg/gameplay.oac`, default
  off). Street buildings (fqon contains `"street"` / `"road"`) register their
  tile on spawn; `Move::move_default` applies `STREET_MOVE_MULT` (default 1.25)
  via `GameState::get_tile_move_speed_multiplier` per waypoint segment. Placement
  requires a free land tile (`can_place_street`). Destroying the building clears
  the registration. Tests: `streets_move_speed_multiplier`, `streets_lifecycle`,
  `building_kind_helpers`.
- [x] **Bridges** — opt-in via `GAMEPLAY_BRIDGES`. Bridge buildings (fqon contains
  `"bridge"`) register their tile on spawn. Each path query re-applies bridge
  costs after `restore_sector_costs` / hazards: Land grid → `COST_MIN`, Water
  grid → `COST_IMPASSABLE` (`apply_bridge_path_costs`). Placement requires a free
  water tile when Water/Land grids exist. Tests: `bridges_lifecycle`.
  *Note:* cross-sector portal refresh after a bridge opens a new land corridor
  is deferred; same-sector crossings work with the cost overlay.

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
./run test -a          # all registered C++/Python tests pass (exit 0)
```

Notes:
- Ubuntu 24.04's default `python3` may be a 3.11 that lacks numpy/mako; point
  CMake at 3.12 with `-DPython3_EXECUTABLE=/usr/bin/python3.12`. Cython must be
  installed for *that* interpreter.
- `Map` construction is robust to a database without the `engine.util.path_type.PathType`
  base object (it builds with no pathfinding grids) so gamestate unit tests can
  construct a `Map` without loading the full nyan API. This fixed an abort in the
  `fog_tile_texture` test that previously took down the whole `./run test -a` run.

### Audit & Polish (gameplay)

A correctness pass over Phase 1–3 fixed several leaks and combat/placement holes:

- [x] **Population release only when recorded** — destroying a building/unit no
  longer invents `DEFAULT_BUILDING_POPULATION_SPACE` / `DEFAULT_POPULATION_COST`
  when spawn never recorded provision/demand. Test: `population_no_phantom_release`.
- [x] **Auto-attack / attack-move / guard / patrol respect fog** — enemy scans
  gate on `is_entity_visible` so units cannot acquire targets outside LOS.
- [x] **Street/bridge spawn re-validation** — `SpawnProductionHandler` re-runs
  `can_place_*` before registering tiles (construction race / feature toggle).
- [x] **Street/bridge register overwrite** — registering a tile evicts the prior
  owner so destroy cleanup cannot clear a still-active registration. Test:
  `street_tile_overwrite`.
- [x] **Fog last-known cleanup** — `FogOfWar::clear_entity` runs on both
  `remove_game_entity` overloads. Test: `fog_last_known_cleared_on_remove`.
- [x] **Placement vs occupancy** — `can_place_street` / `can_place_bridge` reject
  tiles already occupied by mobile units.
- [x] **Approach movement on out-of-range attack** — When attacking a target beyond
  `max_range`, mobile units now step toward the target via `Move::move_default` and
  re-enqueue `AttackCommand` instead of dropping the order and staying idle.
- [x] **Approach movement and drop-off pathing on gather** — Mobile gatherers now step
  toward distant resources rather than dropping the command, and gatherers carrying cargo
  now navigate to the nearest friendly drop-off building rather than freezing in place.
- [x] **Null map/pathfinder protection in Move** — `Move::move_default` guards against
  null `map` / `pathfinder` before dereferencing, preventing crashes in minimal or
  headless simulations.

## Implementation Notes

- All new commands follow the pattern in
  `libopenage/gamestate/component/internal/commands/` (see `move.h` / `move.cpp`).
- All new API components follow the pattern in
  `libopenage/gamestate/component/api/` (see `move.h`, `live.h`).
- All new systems follow the pattern in
  `libopenage/gamestate/system/` (see `move.h` / `move.cpp`).
- New source files must be added to the relevant `CMakeLists.txt`.
- Damage values, ranges, and rates are stored in **nyan** data objects, not hard-coded.
