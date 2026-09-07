// Copyright 2023-2026 the openage authors. See copying.md for legal info.

#pragma once

#include <algorithm>
#include <cstdint>

#include "coord/phys.h"

/**
 * Hardcoded definitions for parameters used in the gamestate.
 *
 * May be moved to configuration files in the future.
 */
namespace openage::gamestate {

/**
 * Origin point of the game world.
 */
constexpr coord::phys3 WORLD_ORIGIN = coord::phys3{0, 0, 0};

/**
 * Fallback population demand when an entity has no recorded or nyan-sourced
 * value (e.g. test helpers that bypass SpawnProductionHandler).
 */
constexpr int64_t DEFAULT_POPULATION_COST = 1;

/**
 * Hard ceiling on a player's population capacity, regardless of how much
 * headroom their buildings would otherwise provide.
 */
constexpr int64_t POPULATION_MAX = 200;

/**
 * Fallback population headroom when a building has no recorded or nyan-sourced
 * ProvideContingent value (e.g. test helpers that bypass SpawnProductionHandler).
 */
constexpr int64_t DEFAULT_BUILDING_POPULATION_SPACE = 5;

/**
 * Default fraction of construction cost returned as salvage when a building is
 * destroyed (combat, delete, etc.). Overridden per creatable via nyan
 * CreatableGameEntity.salvage_recovery_fraction when set.
 */
constexpr double SALVAGE_RECOVERY_FRACTION = 0.5;

/**
 * Default fraction recovered when a building is deconstructed (slower, higher yield).
 * Overridden per creatable via CreatableGameEntity.deconstruct_recovery_fraction.
 */
constexpr double DECONSTRUCT_RECOVERY_FRACTION = 0.75;

/**
 * Multiplier applied to creation_time when CreatableGameEntity.deconstruct_time is unset.
 */
constexpr double DECONSTRUCT_TIME_FACTOR = 1.5;

/**
 * Seconds of game time between each unit of salvage decay (1 resource / interval).
 */
constexpr double SALVAGE_DECAY_INTERVAL_SEC = 10.0;

/**
 * Whether harvestable resource nodes (e.g. forests, gold) regenerate by default.
 *
 * Disabled keeps the original Age of Empires behaviour: a resource node is
 * removed once it is fully depleted. When enabled (opt-in via the
 * GAMEPLAY_FOREST_REGEN cvar / cfg/gameplay.oac), depleted nodes are kept in
 * the world and slowly regrow back towards the amount they were first tapped at.
 */
constexpr bool FOREST_REGEN_ENABLED_DEFAULT = false;

/**
 * Default seconds of game time between each resource regeneration step for a
 * harvestable resource node when regeneration is enabled.
 */
constexpr double FOREST_REGEN_INTERVAL_SEC = 5.0;

/**
 * Default amount of the resource restored per regeneration step.
 */
constexpr int64_t FOREST_REGEN_AMOUNT = 1;

/**
 * Whether buildable streets are enabled by default.
 *
 * Disabled keeps original movement (no street speed bonus). When enabled
 * (opt-in via GAMEPLAY_STREETS / cfg/gameplay.oac), units travelling over
 * registered street tiles move faster.
 */
constexpr bool STREETS_ENABLED_DEFAULT = false;

/**
 * Movement-speed multiplier applied while a unit is on a street tile.
 */
constexpr double STREET_MOVE_MULT = 1.25;

/**
 * Whether buildable bridges are enabled by default.
 *
 * Disabled keeps water impassable to land units. When enabled (opt-in via
 * GAMEPLAY_BRIDGES / cfg/gameplay.oac), bridge buildings make land pathfinding
 * passable on their tile and block water pathfinding there.
 */
constexpr bool BRIDGES_ENABLED_DEFAULT = false;

/**
 * Whether the day/night cycle is enabled by default.
 *
 * Disabled keeps constant daytime sight (original behaviour). When enabled
 * (opt-in via GAMEPLAY_DAY_NIGHT / cfg/gameplay.oac), line-of-sight shrinks
 * at night and during twilight.
 */
constexpr bool DAY_NIGHT_ENABLED_DEFAULT = false;

/**
 * Default length of daytime in seconds of game time.
 */
constexpr double DAY_LENGTH_SEC = 300.0;

/**
 * Default length of nighttime in seconds of game time.
 */
constexpr double NIGHT_LENGTH_SEC = 180.0;

/**
 * Sight-range multipliers for each day phase (applied to DEFAULT_SIGHT_RANGE).
 */
constexpr double DAY_SIGHT_MULT = 1.0;
constexpr double TWILIGHT_SIGHT_MULT = 0.75;
constexpr double NIGHT_SIGHT_MULT = 0.5;

/**
 * Fraction of the day/night segment used as dusk/dawn transition.
 */
constexpr double TWILIGHT_FRACTION = 0.1;

/**
 * Whether dynamic weather is enabled by default.
 *
 * Disabled keeps clear weather (original behaviour). When enabled (opt-in via
 * GAMEPLAY_WEATHER / cfg/gameplay.oac), fog and rain reduce sight and rain
 * slows movement.
 */
constexpr bool WEATHER_ENABLED_DEFAULT = false;

/**
 * Seconds between automatic weather transitions when weather cycling is on.
 */
constexpr double WEATHER_CYCLE_INTERVAL_SEC = 120.0;

/**
 * Sight-range multipliers for weather conditions (stacked with day/night).
 */
constexpr double WEATHER_CLEAR_SIGHT_MULT = 1.0;
constexpr double WEATHER_FOG_SIGHT_MULT = 0.6;
constexpr double WEATHER_RAIN_SIGHT_MULT = 0.8;

/**
 * Movement-speed multipliers for weather conditions.
 */
constexpr double WEATHER_CLEAR_MOVE_MULT = 1.0;
constexpr double WEATHER_FOG_MOVE_MULT = 1.0;
constexpr double WEATHER_RAIN_MOVE_MULT = 0.85;

/**
 * Whether forest hiding is enabled by default.
 *
 * Disabled keeps all fog-visible units visible (original behaviour). When
 * enabled (opt-in via GAMEPLAY_FOREST_HIDE / cfg/gameplay.oac), enemy units
 * standing on forest tiles are invisible beyond FOREST_HIDE_THRESHOLD_TILES.
 */
constexpr bool FOREST_HIDE_ENABLED_DEFAULT = false;

/**
 * Chebyshev distance (tiles) within which a forest-hidden unit remains visible.
 */
constexpr int FOREST_HIDE_THRESHOLD_TILES = 2;

/**
 * Maximum distance (in tiles) for builders to start build or deconstruct actions.
 */
constexpr double BUILDER_INTERACTION_RANGE = 2.0;

/**
 * Base repair rate for buildings in HP per second (750 HP / 60 s = 12.5 HP/s).
 * Ref: doc/reverse_engineering/game_mechanics/repair.md
 */
constexpr double BUILDING_REPAIR_HP_PER_SEC = 12.5;

/**
 * Base repair rate for mobile units/siege/ships in HP per second (188 HP / 60 s = 3.133 HP/s).
 */
constexpr double UNIT_REPAIR_HP_PER_SEC = 3.133;

/**
 * Interaction range for repair actions (matches BUILDER_INTERACTION_RANGE).
 */
constexpr double REPAIR_INTERACTION_RANGE = BUILDER_INTERACTION_RANGE;

/**
 * Fallback building maximum HP when not explicitly recorded.
 */
constexpr int64_t DEFAULT_BUILDING_MAX_HP = 1000;

/**
 * Fallback unit maximum HP when not explicitly recorded.
 */
constexpr int64_t DEFAULT_UNIT_MAX_HP = 100;

/**
 * Standard garrison capacity for Town Centers.
 */
constexpr int64_t GARRISON_CAPACITY_TOWN_CENTER = 15;

/**
 * Standard garrison capacity for Castles.
 */
constexpr int64_t GARRISON_CAPACITY_CASTLE = 20;

/**
 * Standard garrison capacity for Watch Towers / Guard Towers / Keeps.
 */
constexpr int64_t GARRISON_CAPACITY_TOWER = 5;

/**
 * Standard garrison capacity for Battering Rams.
 */
constexpr int64_t GARRISON_CAPACITY_RAM = 4;

/**
 * Interaction range for entering a garrisonable building or vehicle.
 */
constexpr double GARRISON_INTERACTION_RANGE = BUILDER_INTERACTION_RANGE;

/**
 * Effective search range for Town Bell activation (25 tiles from Town Center).
 */
constexpr double TOWN_BELL_RANGE_TILES = 25.0;

/**
 * Passive health regeneration rate for garrisoned units (HP per second).
 */
constexpr double GARRISON_HEAL_HP_PER_SEC = 0.1;

/**
 * AoE2 Armor classes (ref: doc/reverse_engineering/game_mechanics/damage.md).
 */
enum class armor_class_t : uint8_t {
	NONE = 0,
	INFANTRY = 1,
	SPEARMAN = 2,
	PIERCE = 3,
	MELEE = 4,
	WAR_ELEPHANT = 5,
	UNARMORED = 6,
	MONK = 7,
	CAVALRY = 8,
	SIEGE_WEAPON = 13,
	ARCHER = 15,
	EAGLE_WARRIOR = 16,
	RAM = 17,
	BUILDING = 11,
	SHIP = 20,
	WALL_GATE = 27,
	CAMEL = 30,
};

/**
 * Default resistance value for armor classes not possessed by a unit/building.
 * In AoE2, units have 1000 armor in unpossessed classes to prevent irrelevant bonus damage.
 */
constexpr int64_t DEFAULT_UNPOSSESSED_ARMOR = 1000;

/**
 * Downhill / high ground damage multiplier (+25%).
 */
constexpr double ELEVATION_DAMAGE_BONUS = 1.25;

/**
 * Uphill / low ground damage multiplier (-25%).
 */
constexpr double ELEVATION_DAMAGE_MALUS = 0.75;

/**
 * Height difference (z-axis) threshold to trigger elevation damage modifiers.
 */
constexpr double ELEVATION_THRESHOLD = 0.25;



/**
 * Phase of the day/night cycle.
 */
enum class day_phase_t : uint8_t {
	DAY = 0,
	DUSK = 1,
	NIGHT = 2,
	DAWN = 3,
};

/**
 * Active weather condition.
 */
enum class weather_t : uint8_t {
	CLEAR = 0,
	FOG = 1,
	RAIN = 2,
};

/**
 * Clamp a resource recovery fraction to [0, 1].
 */
inline double clamp_recovery_fraction(double fraction) {
	return std::clamp(fraction, 0.0, 1.0);
}

/**
 * Tradable and tributable commodity resources in the Market.
 */
enum class market_resource_t : uint8_t {
	FOOD = 0,
	WOOD = 1,
	STONE = 2,
	GOLD = 3,
};

/**
 * Starting base price for wood at the Market in AoE2.
 */
constexpr int64_t MARKET_DEFAULT_BASE_PRICE_WOOD = 100;

/**
 * Starting base price for food at the Market in AoE2.
 */
constexpr int64_t MARKET_DEFAULT_BASE_PRICE_FOOD = 100;

/**
 * Starting base price for stone at the Market in AoE2.
 */
constexpr int64_t MARKET_DEFAULT_BASE_PRICE_STONE = 130;

/**
 * Standard market transaction fee (30%).
 */
constexpr double MARKET_DEFAULT_FEE = 0.30;

/**
 * Price shift in gold per 100 units traded (+3 on buy, -3 on sell).
 */
constexpr int64_t MARKET_PRICE_SHIFT_PER_100 = 3;

/**
 * Minimum base price for any commodity at the Market.
 */
constexpr int64_t MARKET_MIN_BASE_PRICE = 20;

/**
 * Maximum base price for any commodity at the Market.
 */
constexpr int64_t MARKET_MAX_BASE_PRICE = 9999;

/**
 * Standard batch size in units for market buy/sell transactions.
 */
constexpr int64_t MARKET_UNIT_BATCH = 100;

/**
 * Standard tribute tax fee (30%).
 */
constexpr double TRIBUTE_DEFAULT_FEE = 0.30;

/**
 * Interaction range in tiles for trade units (Trade Carts / Cogs) to interact with markets.
 */
constexpr double TRADE_INTERACTION_RANGE = 2.5;

/**
 * Default map width/height in tiles used for trade cart distance formula if map is not queried.
 */
constexpr double DEFAULT_MAP_SIZE_TILES = 120.0;

/**
 * Historical Age progression in Age of Empires II.
 */
enum class age_t : uint8_t {
	DARK_AGE = 0,
	FEUDAL_AGE = 1,
	CASTLE_AGE = 2,
	IMPERIAL_AGE = 3,
};

// Canonical Technology IDs (see doc/reverse_engineering/networking/technology_ids.md)
constexpr int64_t TECH_FEUDAL_AGE = 101;
constexpr int64_t TECH_CASTLE_AGE = 102;
constexpr int64_t TECH_IMPERIAL_AGE = 103;
constexpr int64_t TECH_LOOM = 22;
constexpr int64_t TECH_TOWN_WATCH = 8;
constexpr int64_t TECH_WHEELBARROW = 213;
constexpr int64_t TECH_HAND_CART = 249;
constexpr int64_t TECH_COINAGE = 23;
constexpr int64_t TECH_BANKING = 17;
constexpr int64_t TECH_GUILDS = 15;
constexpr int64_t TECH_FORGING = 67;
constexpr int64_t TECH_IRON_CASTING = 68;
constexpr int64_t TECH_SCALE_MAIL_ARMOR = 74;

// Standard Age Advancement costs and research times
constexpr int64_t FEUDAL_AGE_COST_FOOD = 500;
constexpr double FEUDAL_AGE_RESEARCH_TIME_SEC = 130.0;

constexpr int64_t CASTLE_AGE_COST_FOOD = 800;
constexpr int64_t CASTLE_AGE_COST_GOLD = 200;
constexpr double CASTLE_AGE_RESEARCH_TIME_SEC = 160.0;

constexpr int64_t IMPERIAL_AGE_COST_FOOD = 1000;
constexpr int64_t IMPERIAL_AGE_COST_GOLD = 800;
constexpr double IMPERIAL_AGE_RESEARCH_TIME_SEC = 190.0;

constexpr int64_t LOOM_COST_GOLD = 50;
constexpr double LOOM_RESEARCH_TIME_SEC = 25.0;

constexpr double TRIBUTE_FEE_COINAGE = 0.20;
constexpr double TRIBUTE_FEE_BANKING = 0.00;
constexpr double MARKET_FEE_GUILDS = 0.15;

} // namespace openage::gamestate
