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
# capture JPEGs and the light map file (TerrainLight.OZJ) are data code without
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
# tab's filters and sort order, what the preview's character wears for an item,
# and the A/B compare's candidates (deliveries, pilot variants, folders). File-only code as well; MuClient supplies the engine's class rule
# (GameLogic::Items::CanClassEquip) the class filter and the preview call.
mu_add_test(
    NAME editor_item_browse_tests
    SOURCES
        test_item_browse.cpp
        test_item_candidates.cpp
        test_item_catalog.cpp
        test_preview_outfit.cpp
        ${PROJECT_SOURCE_DIR}/src/MuEditor/Editing/PreviewOutfit.cpp
        ${MU_EDITOR_ASSETS_DIR}/EditorText.cpp
        ${MU_EDITOR_ASSETS_DIR}/FileDigest.cpp
        ${MU_EDITOR_ASSETS_DIR}/ItemBrowse.cpp
        ${MU_EDITOR_ASSETS_DIR}/ItemCandidates.cpp
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
