# Editor-specific tests only.
# Shared test harness setup (doctest include, wine setup, mu_test_main,
# and mu_add_test) lives in the top-level tests/CMakeLists.txt.

# Guard against the editor leaking into a non-editor build (issue #359).
# When ENABLE_EDITOR=OFF, nothing under MuEditor/ should be compiled in. The
# editor sources are added to the MuClient library (Main is just the
# entry-point executable that links it), so MuClient is the target to inspect
# -- Main's own SOURCES never list them.
if(NOT ENABLE_EDITOR)
    # file(GENERATE) resolves the SOURCES generator expression at generate
    # time and writes one absolute path per line.
    file(GENERATE
        OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/muclient_sources.txt"
        CONTENT "$<JOIN:$<TARGET_PROPERTY:MuClient,SOURCES>,\n>")

    # Pure-CMake check: no Python or other interpreter to find, so the test
    # always runs wherever this project builds (CMake is always present),
    # instead of silently skipping when an interpreter is missing.
    # PROJECT_SOURCE_DIR (not CMAKE_SOURCE_DIR) so the editor path stays
    # correct even if MuMain is ever pulled in as a subdirectory of a larger
    # CMake project. Arguments are quoted to survive paths containing spaces.
    add_test(
        NAME editor_leak
        COMMAND ${CMAKE_COMMAND}
            "-DSOURCES_FILE=${CMAKE_CURRENT_BINARY_DIR}/muclient_sources.txt"
            "-DEDITOR_DIR=${PROJECT_SOURCE_DIR}/src/MuEditor"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/test_leak.cmake")
endif()

# Where the editors copy their saves (the repository's src/bin/Data, with
# backups) is plain file-system code, so it is tested on its own, also in
# builds without the editor. The test compiles that one file itself; nothing
# of the editor goes into MuClient.
mu_add_test(
    NAME editor_repo_mirror_tests
    SOURCES
        test_repo_mirror.cpp
        ${PROJECT_SOURCE_DIR}/src/MuEditor/Core/RepoMirror.cpp
        ${PROJECT_SOURCE_DIR}/src/MuEditor/Assets/EditorText.cpp
)
target_compile_definitions(editor_repo_mirror_tests PRIVATE _EDITOR)
target_include_directories(editor_repo_mirror_tests PRIVATE ${PROJECT_SOURCE_DIR}/src/MuEditor)

# The world editor's asset catalog, regeneration request files, git HEAD lookup,
# capture JPEGs, the light map file (TerrainLight.OZJ) and the height file
# (TerrainHeight.OZB, read back by map-revert) are data code without
# ImGui or engine state, so they are tested here too, also in builds without the
# editor. MuClient only supplies OpenSSL (SHA-256) and turbojpeg with their
# include paths.
set(MU_EDITOR_ASSETS_DIR ${PROJECT_SOURCE_DIR}/src/MuEditor/Assets)
mu_add_test(
    NAME editor_asset_request_tests
    SOURCES
        test_asset_catalog.cpp
        test_editor_text.cpp
        test_regen_request.cpp
        test_terrain_light_file.cpp
        ${MU_EDITOR_ASSETS_DIR}/AssetCatalog.cpp
        ${MU_EDITOR_ASSETS_DIR}/CaptureImage.cpp
        ${MU_EDITOR_ASSETS_DIR}/ClientReview.cpp
        ${MU_EDITOR_ASSETS_DIR}/EditorText.cpp
        ${MU_EDITOR_ASSETS_DIR}/FileDigest.cpp
        ${MU_EDITOR_ASSETS_DIR}/GitCheckout.cpp
        ${MU_EDITOR_ASSETS_DIR}/JsonFields.cpp
        ${MU_EDITOR_ASSETS_DIR}/RegenRequest.cpp
        ${MU_EDITOR_ASSETS_DIR}/RequestBrief.cpp
        ${MU_EDITOR_ASSETS_DIR}/RequestFolder.cpp
        ${MU_EDITOR_ASSETS_DIR}/RequestNaming.cpp
        ${MU_EDITOR_ASSETS_DIR}/TerrainHeightFile.cpp
        ${MU_EDITOR_ASSETS_DIR}/TerrainLightFile.cpp
    LINK_LIBS
        MuClient
)
target_compile_definitions(editor_asset_request_tests PRIVATE
    _EDITOR
    "MU_REPO_ROOT=\"${PROJECT_SOURCE_DIR}\""
)
target_include_directories(editor_asset_request_tests PRIVATE
    ${PROJECT_SOURCE_DIR}/src/MuEditor
    ${PROJECT_SOURCE_DIR}/src/ThirdParty
)

# The Item Editor's item catalog (assets-work/Items/catalog.json), its Browse
# tab's filters and sort order, and what the preview's character wears for an
# item. File-only code as well; MuClient supplies the engine's class rule
# (GameLogic::Items::CanClassEquip) the class filter and the preview call.
mu_add_test(
    NAME editor_item_browse_tests
    SOURCES
        test_item_browse.cpp
        test_item_catalog.cpp
        test_preview_outfit.cpp
        ${PROJECT_SOURCE_DIR}/src/MuEditor/Editing/PreviewOutfit.cpp
        ${MU_EDITOR_ASSETS_DIR}/EditorText.cpp
        ${MU_EDITOR_ASSETS_DIR}/ItemBrowse.cpp
        ${MU_EDITOR_ASSETS_DIR}/ItemCatalog.cpp
        ${MU_EDITOR_ASSETS_DIR}/JsonFields.cpp
    LINK_LIBS
        MuClient
)
target_compile_definitions(editor_item_browse_tests PRIVATE
    _EDITOR
    "MU_REPO_ROOT=\"${PROJECT_SOURCE_DIR}\""
)
target_include_directories(editor_item_browse_tests PRIVATE
    ${PROJECT_SOURCE_DIR}/src/MuEditor
    ${PROJECT_SOURCE_DIR}/src/ThirdParty
)

# The Item Editor's item requests (assets-work/Items/requests): request.json and
# brief.md from catalog facts, the folder with captures and reference images,
# the requests scan, the owner's withdraw / accept / reject files and the capture
# plan. File-only code; with python3 the written folders also go through the
# contract's validate_request.py. MuClient supplies OpenSSL and turbojpeg.
mu_add_test(
    NAME editor_item_request_tests
    SOURCES
        test_item_request.cpp
        ${PROJECT_SOURCE_DIR}/src/MuEditor/Editing/ItemCapturePlan.cpp
        ${MU_EDITOR_ASSETS_DIR}/AssetCatalog.cpp
        ${MU_EDITOR_ASSETS_DIR}/CaptureImage.cpp
        ${MU_EDITOR_ASSETS_DIR}/EditorText.cpp
        ${MU_EDITOR_ASSETS_DIR}/FileDigest.cpp
        ${MU_EDITOR_ASSETS_DIR}/GitCheckout.cpp
        ${MU_EDITOR_ASSETS_DIR}/ItemCatalog.cpp
        ${MU_EDITOR_ASSETS_DIR}/ItemRequest.cpp
        ${MU_EDITOR_ASSETS_DIR}/ItemRequestBrief.cpp
        ${MU_EDITOR_ASSETS_DIR}/ItemRequestDecision.cpp
        ${MU_EDITOR_ASSETS_DIR}/ItemRequestFolder.cpp
        ${MU_EDITOR_ASSETS_DIR}/ItemRequestScan.cpp
        ${MU_EDITOR_ASSETS_DIR}/JsonFields.cpp
        ${MU_EDITOR_ASSETS_DIR}/RegenRequest.cpp
        ${MU_EDITOR_ASSETS_DIR}/RequestBrief.cpp
        ${MU_EDITOR_ASSETS_DIR}/RequestFolder.cpp
        ${MU_EDITOR_ASSETS_DIR}/RequestNaming.cpp
    LINK_LIBS
        MuClient
)
target_compile_definitions(editor_item_request_tests PRIVATE
    _EDITOR
    "MU_REPO_ROOT=\"${PROJECT_SOURCE_DIR}\""
)
target_include_directories(editor_item_request_tests PRIVATE
    ${PROJECT_SOURCE_DIR}/src/MuEditor
    ${PROJECT_SOURCE_DIR}/src/ThirdParty
)

# The Item Editor's concept images: the concepts.py command lines, its plan and
# list answers, the run's JSON Lines events as a job's state, and Browse's
# multi-selection. File-only code; nothing starts Python or talks to the API.
mu_add_test(
    NAME editor_item_concepts_tests
    SOURCES
        test_item_concepts.cpp
        ${PROJECT_SOURCE_DIR}/src/MuEditor/Editing/ItemSelection.cpp
        ${MU_EDITOR_ASSETS_DIR}/ConceptCommands.cpp
        ${MU_EDITOR_ASSETS_DIR}/ConceptJobState.cpp
        ${MU_EDITOR_ASSETS_DIR}/ConceptListing.cpp
        ${MU_EDITOR_ASSETS_DIR}/ConceptPlan.cpp
        ${MU_EDITOR_ASSETS_DIR}/EditorText.cpp
        ${MU_EDITOR_ASSETS_DIR}/JsonFields.cpp
    LINK_LIBS
        MuClient
)
target_compile_definitions(editor_item_concepts_tests PRIVATE _EDITOR)
target_include_directories(editor_item_concepts_tests PRIVATE
    ${PROJECT_SOURCE_DIR}/src/MuEditor
    ${PROJECT_SOURCE_DIR}/src/ThirdParty
)

# The world editor's A/B compare checks a model and its textures before the
# running client loads them in place of the one it shows, and finds the
# original files built by tools/world_editor/materialize_variant.py. File-only
# code as well; MuClient supplies turbojpeg, OpenSSL and the map-file cipher the
# test uses to read the shipped models.
mu_add_test(
    NAME editor_model_preflight_tests
    SOURCES
        test_model_preflight.cpp
        ${MU_EDITOR_ASSETS_DIR}/AssetCatalog.cpp
        ${MU_EDITOR_ASSETS_DIR}/AssetVariant.cpp
        ${MU_EDITOR_ASSETS_DIR}/CaptureImage.cpp
        ${MU_EDITOR_ASSETS_DIR}/EditorText.cpp
        ${MU_EDITOR_ASSETS_DIR}/FileDigest.cpp
        ${MU_EDITOR_ASSETS_DIR}/JsonFields.cpp
        ${MU_EDITOR_ASSETS_DIR}/ModelPreflight.cpp
    LINK_LIBS
        MuClient
)
target_compile_definitions(editor_model_preflight_tests PRIVATE
    _EDITOR
    "MU_REPO_ROOT=\"${PROJECT_SOURCE_DIR}\""
)
target_include_directories(editor_model_preflight_tests PRIVATE
    ${PROJECT_SOURCE_DIR}/src/MuEditor
    ${PROJECT_SOURCE_DIR}/src/ThirdParty
)

# The Map Editor's undo history (commands, terrain strokes), object selection,
# transform gizmo math, terrain brushes, popup mouse guard and the model
# thumbnails' camera framing and the Item Editor preview's orbit camera and
# inventory-slot projection: logic without ImGui or engine state, tested
# against stand-in objects and terrain. MuClient supplies the engine's
# AngleMatrix, which the gizmo's turns must agree with.
set(MU_EDITOR_EDITING_DIR ${PROJECT_SOURCE_DIR}/src/MuEditor/Editing)
mu_add_test(
    NAME editor_editing_tests
    SOURCES
        test_edit_history.cpp
        test_gizmo_math.cpp
        test_object_selection.cpp
        test_popup_mouse_guard.cpp
        test_preview_camera.cpp
        test_terrain_brush.cpp
        test_thumbnail_framing.cpp
        ${MU_EDITOR_EDITING_DIR}/CommandStack.cpp
        ${MU_EDITOR_EDITING_DIR}/EditCommandGroup.cpp
        ${MU_EDITOR_EDITING_DIR}/FieldBrush.cpp
        ${MU_EDITOR_EDITING_DIR}/GizmoMath.cpp
        ${MU_EDITOR_EDITING_DIR}/ObjectEditCommand.cpp
        ${MU_EDITOR_EDITING_DIR}/ObjectSelection.cpp
        ${MU_EDITOR_EDITING_DIR}/ObjectTransform.cpp
        ${MU_EDITOR_EDITING_DIR}/PopupMouseGuard.cpp
        ${MU_EDITOR_EDITING_DIR}/PreviewCamera.cpp
        ${MU_EDITOR_EDITING_DIR}/PreviewSlot.cpp
        ${MU_EDITOR_EDITING_DIR}/SurfaceBrush.cpp
        ${MU_EDITOR_EDITING_DIR}/TerrainBrush.cpp
        ${MU_EDITOR_EDITING_DIR}/TerrainPatchCommand.cpp
        ${MU_EDITOR_EDITING_DIR}/TerrainStroke.cpp
        ${MU_EDITOR_EDITING_DIR}/ThumbnailFraming.cpp
    LINK_LIBS
        MuClient
)
target_compile_definitions(editor_editing_tests PRIVATE _EDITOR)
target_include_directories(editor_editing_tests PRIVATE ${PROJECT_SOURCE_DIR}/src/MuEditor)

# The Map Editor's read-only map inspection for scripted clients (map-export,
# map-query, map-camera's framing, map-info's unsaved edits): layer images, legends,
# statistics, framing math and fingerprints, without ImGui or engine state. MuClient
# supplies the object file's save order (WorldObjectFile) and the PNG writer's header.
set(MU_EDITOR_INSPECT_DIR ${PROJECT_SOURCE_DIR}/src/MuEditor/MapInspect)
mu_add_test(
    NAME editor_map_inspect_tests
    SOURCES
        test_map_inspect.cpp
        ${MU_EDITOR_INSPECT_DIR}/AreaStats.cpp
        ${MU_EDITOR_INSPECT_DIR}/AttributePalette.cpp
        ${MU_EDITOR_INSPECT_DIR}/CameraFraming.cpp
        ${MU_EDITOR_INSPECT_DIR}/Image.cpp
        ${MU_EDITOR_INSPECT_DIR}/LayerImage.cpp
        ${MU_EDITOR_INSPECT_DIR}/MapDigest.cpp
        ${MU_EDITOR_INSPECT_DIR}/MapExport.cpp
        ${MU_EDITOR_INSPECT_DIR}/MapObjectList.cpp
        ${MU_EDITOR_INSPECT_DIR}/PngFile.cpp
        ${MU_EDITOR_INSPECT_DIR}/SavedMapState.cpp
        ${MU_EDITOR_INSPECT_DIR}/TileArea.cpp
        ${MU_EDITOR_INSPECT_DIR}/TilePalette.cpp
        ${MU_EDITOR_ASSETS_DIR}/EditorText.cpp
    LINK_LIBS
        MuClient
)
target_compile_definitions(editor_map_inspect_tests PRIVATE _EDITOR)
target_include_directories(editor_map_inspect_tests PRIVATE
    ${PROJECT_SOURCE_DIR}/src/MuEditor
    ${PROJECT_SOURCE_DIR}/src/ThirdParty
)

# The edit scripts an AI agent applies through the control socket's map-apply: parsing
# and validation, shapes, seeded scatter and noise, and every op on plain arrays, without
# ImGui or engine state. MuClient supplies the engine's AngleMatrix, which object turns
# go through (Editing/GizmoMath).
set(MU_EDITOR_SCRIPT_DIR ${PROJECT_SOURCE_DIR}/src/MuEditor/MapScript)
mu_add_test(
    NAME editor_map_script_tests
    SOURCES
        test_map_script.cpp
        ${MU_EDITOR_SCRIPT_DIR}/AttributeRules.cpp
        ${MU_EDITOR_SCRIPT_DIR}/EditScript.cpp
        ${MU_EDITOR_SCRIPT_DIR}/NameResolver.cpp
        ${MU_EDITOR_SCRIPT_DIR}/ObjectOpParser.cpp
        ${MU_EDITOR_SCRIPT_DIR}/ObjectOps.cpp
        ${MU_EDITOR_SCRIPT_DIR}/PointGrid.cpp
        ${MU_EDITOR_SCRIPT_DIR}/PoissonScatter.cpp
        ${MU_EDITOR_SCRIPT_DIR}/ScatterRules.cpp
        ${MU_EDITOR_SCRIPT_DIR}/ScriptCost.cpp
        ${MU_EDITOR_SCRIPT_DIR}/ScriptDiff.cpp
        ${MU_EDITOR_SCRIPT_DIR}/ScriptMap.cpp
        ${MU_EDITOR_SCRIPT_DIR}/ScriptParser.cpp
        ${MU_EDITOR_SCRIPT_DIR}/ScriptRandom.cpp
        ${MU_EDITOR_SCRIPT_DIR}/ScriptReader.cpp
        ${MU_EDITOR_SCRIPT_DIR}/ScriptReport.cpp
        ${MU_EDITOR_SCRIPT_DIR}/ScriptRunner.cpp
        ${MU_EDITOR_SCRIPT_DIR}/ScriptShape.cpp
        ${MU_EDITOR_SCRIPT_DIR}/ShapeParser.cpp
        ${MU_EDITOR_SCRIPT_DIR}/SurfaceOpParser.cpp
        ${MU_EDITOR_SCRIPT_DIR}/SurfaceOps.cpp
        ${MU_EDITOR_SCRIPT_DIR}/TerrainOps.cpp
        ${MU_EDITOR_SCRIPT_DIR}/ValueNoise.cpp
        ${MU_EDITOR_EDITING_DIR}/FieldBrush.cpp
        ${MU_EDITOR_EDITING_DIR}/GizmoMath.cpp
        ${MU_EDITOR_EDITING_DIR}/ObjectTransform.cpp
        ${MU_EDITOR_EDITING_DIR}/SurfaceBrush.cpp
        ${MU_EDITOR_EDITING_DIR}/TerrainBrush.cpp
        ${MU_EDITOR_INSPECT_DIR}/Image.cpp
        ${MU_EDITOR_INSPECT_DIR}/TilePalette.cpp
    LINK_LIBS
        MuClient
)
target_compile_definitions(editor_map_script_tests PRIVATE _EDITOR)
target_include_directories(editor_map_script_tests PRIVATE
    ${PROJECT_SOURCE_DIR}/src/MuEditor
    ${PROJECT_SOURCE_DIR}/src/ThirdParty
)
# Built as the client builds the MapScript units (src/CMakeLists.txt): no fused multiply-add,
# so a seed gives the same map with every compiler.
if(NOT MSVC)
    target_compile_options(editor_map_script_tests PRIVATE -ffp-contract=off)
endif()

# Growing the world (M10): Gate.bmd's reader and writer and the gate edits, the files of a
# new map (renumbered copies, flat maps, Lorencia's renamed models) and the OpenMU export,
# without ImGui or engine state. MuClient supplies the object file layout, the terrain
# file layouts, the map name file parser, turbojpeg for the light map, and the ciphers the
# tests hand in.
set(MU_EDITOR_GATES_DIR ${PROJECT_SOURCE_DIR}/src/MuEditor/Gates)
set(MU_EDITOR_NEW_MAP_DIR ${PROJECT_SOURCE_DIR}/src/MuEditor/NewMap)
set(MU_EDITOR_SERVER_EXPORT_DIR ${PROJECT_SOURCE_DIR}/src/MuEditor/ServerExport)
mu_add_test(
    NAME editor_world_growth_tests
    SOURCES
        test_gates.cpp
        test_new_map.cpp
        ${MU_EDITOR_GATES_DIR}/GateChecks.cpp
        ${MU_EDITOR_GATES_DIR}/GateDirection.cpp
        ${MU_EDITOR_GATES_DIR}/GateFile.cpp
        ${MU_EDITOR_GATES_DIR}/GateQueries.cpp
        ${MU_EDITOR_GATES_DIR}/GateRecord.cpp
        ${MU_EDITOR_GATES_DIR}/GateTableEdit.cpp
        ${MU_EDITOR_NEW_MAP_DIR}/BlankMap.cpp
        ${MU_EDITOR_NEW_MAP_DIR}/EncTerrainFile.cpp
        ${MU_EDITOR_NEW_MAP_DIR}/NamedModels.cpp
        ${MU_EDITOR_NEW_MAP_DIR}/NewMapPlan.cpp
        ${MU_EDITOR_NEW_MAP_DIR}/NewMapWriter.cpp
        ${MU_EDITOR_NEW_MAP_DIR}/WorldFolders.cpp
        ${MU_EDITOR_SERVER_EXPORT_DIR}/ExportHowTo.cpp
        ${MU_EDITOR_SERVER_EXPORT_DIR}/ExportInput.cpp
        ${MU_EDITOR_SERVER_EXPORT_DIR}/ExportSql.cpp
        ${MU_EDITOR_SERVER_EXPORT_DIR}/OpenMuDocuments.cpp
        ${MU_EDITOR_SERVER_EXPORT_DIR}/ServerTerrain.cpp
        ${MU_EDITOR_ASSETS_DIR}/EditorText.cpp
        ${MU_EDITOR_ASSETS_DIR}/TerrainHeightFile.cpp
        ${MU_EDITOR_ASSETS_DIR}/TerrainLightFile.cpp
        ${MU_EDITOR_INSPECT_DIR}/Image.cpp
        ${MU_EDITOR_INSPECT_DIR}/TilePalette.cpp
        ${MU_EDITOR_SCRIPT_DIR}/AttributeRules.cpp
        ${PROJECT_SOURCE_DIR}/src/MuEditor/Core/RepoMirror.cpp
    LINK_LIBS
        MuClient
)
target_compile_definitions(editor_world_growth_tests PRIVATE
    _EDITOR
    "MU_REPO_ROOT=\"${PROJECT_SOURCE_DIR}\""
)
target_include_directories(editor_world_growth_tests PRIVATE
    ${PROJECT_SOURCE_DIR}/src/MuEditor
    ${PROJECT_SOURCE_DIR}/src/ThirdParty
)

# mapctl, the command line an AI agent drives the Map Editor with over the control socket
# (tools/world_editor/mapctl.py): argument parsing, request framing against a fake socket and
# the plan sketches. Plain Python without the client, so it runs in every build; -B keeps
# bytecode caches out of the source tree.
add_test(
    NAME world_editor_mapctl
    COMMAND "${Python3_EXECUTABLE}" -B "${PROJECT_SOURCE_DIR}/tools/world_editor/tests/test_mapctl.py"
)

# The Item Editor's save of the item table (Data/Local/<lang>/Item_<lang>.bmd).
# Saving is editor-only code in MuClient, so the test exists only in editor
# builds. It loads the shipped tables and saves them into a temporary folder:
# without an edit the file must come back byte for byte (layout, junk bytes,
# checksum), with an edit only that record and the checksum may change.
# Winmain.cpp defines globals the loader's code reaches (as in tests/engine).
if(ENABLE_EDITOR)
    mu_add_test(
        NAME editor_item_table_tests
        SOURCES
            test_item_table_file.cpp
            ${PROJECT_SOURCE_DIR}/src/source/App/Platform/Windows/Winmain.cpp
        LINK_LIBS
            MuClient
    )
    target_compile_definitions(editor_item_table_tests PRIVATE
        "MU_TEST_DATA_DIR=\"${PROJECT_SOURCE_DIR}/src/bin/Data/\""
    )
endif()
