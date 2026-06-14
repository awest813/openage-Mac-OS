# Copyright 2015-2024 the openage authors. See copying.md for legal info.

""" Lists of all possible tests; enter your tests here. """


def doctest_modules():
    """
    Yields the names of all Python modules that shall be tested during doctest.
    """

    yield "openage.util.math"
    yield "openage.util.strings"
    yield "openage.util.system"
    yield "openage.util.version"


def tests_py():
    """
    Yields tuples of (name, description, condition_function)
    for all Python test methods.

    If no description is required, just the name may be yielded.
    """

    yield ("openage.testing.doctest.test",
           "doctest on all modules from DOCTEST_MODULES")
    yield "openage.assets.test"
    yield ("openage.cabextract.test.test", "test CAB archive extraction",
           lambda env: env["has_assets"])
    yield "openage.cppinterface.exctranslate_tests.cpp_to_py"
    yield ("openage.cppinterface.exctranslate_tests.cpp_to_py_bounce",
           "translates the exception back and forth a few times")
    yield ("openage.testing.misc_cpp.enum",
           "tests the interface for C++'s util::Enum class")
    yield ("openage.util.fslike.test.test",
           "test the filesystem abstraction subsystem")
    yield ("openage.default_dirs.test",
           "test macOS/platform directory resolution")
    yield "openage.util.threading.test_concurrent_chain"


def demos_py():
    """
    Yields tuples of (name, description) for all Python demo methods.
    """

    yield ("openage.cppinterface.exctranslate_tests.cpp_to_py_demo",
           "translates a C++ exception and its causes to python")
    yield ("openage.log.tests.demo",
           "demonstrates the translation of Python log messages")
    yield ("openage.convert.service.export.opus.demo.convert",
           "encodes an opus file from a wave file")
    yield ("openage.event.demo.curvepong",
           "play pong on steroids through future prediction")
    yield ("openage.gamestate.tests.simulation_demo",
           "showcases the game simulation")
    yield ("openage.pathfinding.tests.path_demo",
           "showcases the pathfinding system")
    yield ("openage.renderer.tests.renderer_demo",
           "showcases the renderer")
    yield ("openage.renderer.tests.renderer_stresstest",
           "stresstests for the renderer")
    yield ("openage.main.tests.engine_demo",
           "showcases the engine features")


def benchmark_py():
    """
    Yields tuples of (name, description) for python benchmark
    methods.
    """

    # TODO Add a real benchmark here, and remove this one
    yield ("openage.testing.benchmark.benchmark_test_function",
           "Benchmark yourself")


def tests_cpp():
    """
    Yields tuples of (name, description, condition_function)
    for all C++ test methods.

    If no description is required, just the name may be yielded.
    """

    yield "openage::coord::tests::coord"
    yield "openage::datastructure::tests::concurrent_queue"
    yield "openage::datastructure::tests::constexpr_map"
    yield "openage::datastructure::tests::pairing_heap"
    yield "openage::job::tests::test_job_manager"
    yield "openage::path::tests::path_node", "pathfinding"
    yield "openage::path::tests::flow_field", "pathfinding"
    yield "openage::pyinterface::tests::pyobject"
    yield "openage::pyinterface::tests::err_py_to_cpp"
    yield "openage::renderer::tests::font"
    yield "openage::renderer::tests::font_manager"
    yield "openage::rng::tests::run"
    yield "openage::util::tests::constinit_vector"
    yield "openage::util::tests::enum_"
    yield "openage::util::tests::fixed_point"
    yield "openage::util::tests::init"
    yield "openage::util::tests::matrix"
    yield "openage::util::tests::quaternion"
    yield "openage::util::tests::vector"
    yield "openage::util::tests::siphash"
    yield "openage::util::tests::array_conversion"
    yield "openage::curve::tests::container"
    yield "openage::curve::tests::curve_types"
    yield "openage::event::tests::eventtrigger"
    yield "openage::gamestate::tests::carried_resources_lifecycle"
    yield "openage::gamestate::tests::rally_point_lifecycle"
    yield "openage::gamestate::tests::fog_of_war_exploration"
    yield "openage::gamestate::tests::fog_of_war_visibility"
    yield "openage::gamestate::tests::fog_of_war_refresh"
    yield "openage::gamestate::tests::entity_visibility_query"
    yield "openage::gamestate::tests::fog_render_visibility"
    yield "openage::gamestate::tests::fog_tile_texture"
    yield "openage::gamestate::tests::minimap_texture"
    yield "openage::gamestate::tests::hazard_path_costs_no_map"
    yield "openage::gamestate::tests::next_command_conditions"
    yield "openage::gamestate::tests::next_command_conditions_extended"
    yield "openage::gamestate::tests::next_command_formation_move_test"
    yield "openage::gamestate::tests::no_defeat_for_unit_death"
    yield "openage::gamestate::tests::player_defeated_on_last_building_destroyed"
    yield "openage::gamestate::tests::building_population_capacity"
    yield "openage::gamestate::tests::building_cost_and_salvage_spawn"
    yield "openage::gamestate::tests::building_cost_multi_resource_salvage"
    yield "openage::gamestate::tests::building_cost_fraction_clamped"
    yield "openage::gamestate::tests::building_cost_custom_salvage_fraction"
    yield "openage::gamestate::tests::deconstruct_complete_spawns_salvage_if_building_gone"
    yield "openage::gamestate::tests::salvage_decay"
    yield "openage::gamestate::tests::resource_node_regen"
    yield "openage::gamestate::tests::player_resources"
    yield "openage::gamestate::tests::player_population"
    yield "openage::gamestate::tests::population_lookup_missing_entity"
    yield "openage::gamestate::tests::entity_population_tracking"
    yield "openage::gamestate::tests::player_state_transitions"
    yield "openage::gamestate::tests::production_requests"
    yield "openage::gamestate::tests::send_command_variants"
    yield "openage::gamestate::tests::stance_component"
    yield "openage::gamestate::tests::tile_occupancy"
    yield "openage::gamestate::tests::last_known_positions"
    yield "openage::gamestate::tests::next_command_build_test"
    yield "openage::gamestate::tests::build_command_placement"


def demos_cpp():
    """
    Yields tuples of (name, description) for all C++ demo methods.
    """

    yield ("openage::console::tests::render",
           "prints a few test lines to a buffer, and renders it to stdout")
    yield ("openage::console::tests::interactive",
           "showcases console as an interactive terminal on your current tty")
    yield ("openage::error::demo",
           "showcases the openage exceptions, including backtraces")
    yield ("openage::gamestate::tests::activity_demo",
           "showcases the activity system in the gamestate")
    yield ("openage::input::tests::action_demo",
           "showcases the low-level input system")
    yield ("openage::log::tests::demo",
           "showcases the logging system")
    yield ("openage::pyinterface::tests::err_py_to_cpp_demo",
           "translates a Python exception to C++")
    yield ("openage::pyinterface::tests::pyobject_demo",
           "a tiny interactive interpreter using PyObjectRef")


def benchmark_cpp():
    """
    Yields tuples of (name, description) for C++ benchmark
    methods.
    """

    # TODO Add a real benchmark here!
    yield ("openage::test::benchmark", "Test the benchmark")
