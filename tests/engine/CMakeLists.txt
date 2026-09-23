# Engine tests: world objects and terrain. They link the client library, which
# holds the engine's global arrays these functions work on. Tests that reach the
# terrain and object code also compile Winmain.cpp, which defines globals that
# code refers to (as test_audio_playback does).

mu_add_test(
    NAME world_object_file_tests
    SOURCES
        test_world_object_file.cpp
    LINK_LIBS
        MuClient
)
target_compile_definitions(world_object_file_tests PRIVATE
    "MU_TEST_DATA_DIR=\"${CMAKE_SOURCE_DIR}/src/bin/Data/\""
)

mu_add_test(
    NAME terrain_normal_tests
    SOURCES
        test_terrain_normals.cpp
        ${CMAKE_SOURCE_DIR}/src/source/App/Platform/Windows/Winmain.cpp
    LINK_LIBS
        MuClient
)

mu_add_test(
    NAME object_references_tests
    SOURCES
        test_object_references.cpp
        ${CMAKE_SOURCE_DIR}/src/source/App/Platform/Windows/Winmain.cpp
    LINK_LIBS
        MuClient
)

# The class part of IsRequireEquipItem (which classes and stages may equip an
# item), shared with the editor's item browser.
mu_add_test(
    NAME item_class_rule_tests
    SOURCES
        test_item_class_rule.cpp
    LINK_LIBS
        MuClient
)
