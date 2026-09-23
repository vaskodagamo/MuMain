# "MU Item Editor.app": a double-clickable launcher next to Main.app in editor
# builds on macOS (out/build/<preset>/src/<Config>/). Its executable is a shell
# script that runs Main.app's client with "--editor --items" (the Item Editor's
# studio); it reuses the game's icon. Nothing is installed outside the build.
#
# Included from src/CMakeLists.txt after the Main target exists.

set(MU_ITEM_EDITOR_APP_NAME "MU Item Editor")
set(MU_ITEM_EDITOR_ICON_NAME "MuMain.icns")
set(MU_ITEM_EDITOR_TEMPLATES "${CMAKE_CURRENT_SOURCE_DIR}/MuEditor/Launcher")
set(MU_ITEM_EDITOR_STAGING "${CMAKE_CURRENT_BINARY_DIR}/ItemEditorLauncher")

configure_file("${MU_ITEM_EDITOR_TEMPLATES}/ItemEditorInfo.plist.in" "${MU_ITEM_EDITOR_STAGING}/Info.plist" @ONLY)
# Generated per configuration: the script names the Main.app of its own build.
file(GENERATE
    OUTPUT "${MU_ITEM_EDITOR_STAGING}/$<CONFIG>/launcher.sh"
    INPUT "${MU_ITEM_EDITOR_TEMPLATES}/ItemEditorLauncher.sh.in"
    FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)

set(MU_ITEM_EDITOR_APP "$<TARGET_BUNDLE_DIR:Main>/../${MU_ITEM_EDITOR_APP_NAME}.app")
add_custom_target(ItemEditorLauncher ALL
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${MU_ITEM_EDITOR_APP}/Contents/MacOS"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${MU_ITEM_EDITOR_APP}/Contents/Resources"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${MU_ITEM_EDITOR_STAGING}/Info.plist"
            "${MU_ITEM_EDITOR_APP}/Contents/Info.plist"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${MU_ITEM_EDITOR_STAGING}/$<CONFIG>/launcher.sh"
            "${MU_ITEM_EDITOR_APP}/Contents/MacOS/${MU_ITEM_EDITOR_APP_NAME}"
    COMMAND chmod 755 "${MU_ITEM_EDITOR_APP}/Contents/MacOS/${MU_ITEM_EDITOR_APP_NAME}"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${MU_MACOS_ICON}"
            "${MU_ITEM_EDITOR_APP}/Contents/Resources/${MU_ITEM_EDITOR_ICON_NAME}"
    DEPENDS Main
    COMMENT "Building ${MU_ITEM_EDITOR_APP_NAME}.app next to Main.app"
    VERBATIM)
