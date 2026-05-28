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

**Status:** 🚧 In progress

Design: training is modelled on the existing command + system pattern. A
producing entity (e.g. a building) carries a `Create` API component wrapping the
nyan `engine.ability.type.Create` ability. The ability holds a set of
`CreatableGameEntity` objects, each with a `game_entity` (the unit to spawn), a
`creation_time`, and a resource cost (`cost_resource` + `cost_amount`).

A `TRAIN` command carries the fqon of the unit to produce. The `Production`
system finds the matching creatable, verifies the owner can afford it, deducts
the cost from the player's resources, and records a timed production request on
the `GameState`. Because systems cannot reach the `EntityFactory`, the request
queue (`GameState::request_production` / `take_completed_productions`) is the
seam the simulation drains to spawn the finished unit via the `Spawner`.

- [x] Add `TRAIN` command type and `TrainCommand` (carries the unit fqon)
- [x] Add `CREATE` component type and `Create` API component (creatables, cost, build time)
- [x] Add `TRAIN_COMMAND` system id and `next_command_train` condition
- [x] Resource cost check before queuing; deduct resources on train start
- [x] `Production` system: validate creatable, check/deduct cost, queue timed request
- [x] Timed production request queue on `GameState` (`request_production` / `take_completed_productions`)
- [x] Wire `Create` into the entity factory, activity graph, and `send_command`
- [x] Drain completed production requests: `Production::train_command` fires a `"game.spawn_production"` event at `completion_time`; `SpawnProductionHandler` (registered in the simulation) creates and adds the entity at the producer's position when the event fires
- [ ] Add `BUILD` command type for placing new buildings

### 1.4 Win / Loss Conditions

**Status:** ☐ Not started

- [ ] Add a `Player` state flag (alive / defeated / winner)
- [ ] Defeat a player when their Town Center (or last building) is destroyed
- [ ] Broadcast game-over event to all connected clients/UI

---

## Phase 2 — Quality-of-Life Improvements

These improve the experience once Phase 1 is functional.

### 2.1 Unit Behaviour

- [ ] Attack-move command: move toward target, attack enemies encountered along the path
- [ ] Guard / follow command: stay near a target unit and attack threats
- [ ] Attack stances: aggressive, defensive, stand-ground, no-attack
- [ ] Auto-attack: units in aggressive stance automatically target nearby enemies
- [ ] Patrol: cycle between two or more waypoints, auto-attack enemies in range

### 2.2 Pathfinding Improvements

- [ ] Collision avoidance: prevent multiple units occupying the same tile
- [ ] Formation movement: move a group while preserving relative positions
- [ ] Hazard cost modifier: avoid tiles in range of enemy towers/castles
- [ ] Blocked-path re-routing: if the path becomes blocked, recalculate

### 2.3 Interface Improvements

*(see also `doc/ideas/interface.md`)*

- [ ] Select more than 30 units (engine already removes the cap, UI must follow)
- [ ] Customisable hotkeys for all actions
- [ ] Minimap improvements: symbols for key buildings, big-map overlay
- [ ] After-game statistics screen (kills, resources gathered, APM)
- [ ] Zoom towards mouse cursor / selected units

### 2.4 Fog of War

- [ ] Track which tiles each player has explored
- [ ] Track which tiles are currently visible (line-of-sight per unit)
- [ ] Hide enemy units outside visible tiles
- [ ] Show last-known position of units that leave vision

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

- [ ] Resource salvage piles from destroyed buildings
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

## Implementation Notes

- All new commands follow the pattern in
  `libopenage/gamestate/component/internal/commands/` (see `move.h` / `move.cpp`).
- All new API components follow the pattern in
  `libopenage/gamestate/component/api/` (see `move.h`, `live.h`).
- All new systems follow the pattern in
  `libopenage/gamestate/system/` (see `move.h` / `move.cpp`).
- New source files must be added to the relevant `CMakeLists.txt`.
- Damage values, ranges, and rates are stored in **nyan** data objects, not hard-coded.
