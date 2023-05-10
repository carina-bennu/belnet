set(default_build_gui OFF)
set(default_gui_target pack)
if(APPLE)
  set(default_build_gui ON)
  set(default_gui_target macos:raw)
elseif(WIN32)
  set(default_build_gui ON)
  set(default_gui_target win32)
endif()

option(BUILD_GUI "build electron gui from 'gui' submodule source" ${default_build_gui})
set(GUI_YARN_TARGET "${default_gui_target}" CACHE STRING "yarn target for building the GUI")
set(GUI_YARN_EXTRA_OPTS "" CACHE STRING "extra options to pass into the yarn build command")

if (BUILD_GUI)
  message(STATUS "Building belnet-gui")
  if(NOT YARN)
    find_program(YARN NAMES yarn yarnpkg REQUIRED)
  endif()
  message(STATUS "Building belnet-gui with yarn ${YARN}, target ${GUI_YARN_TARGET}")
  set(wine_env)
  if(WIN32)
    set(wine_env USE_SYSTEM_7ZA=true DISPLAY= WINEDEBUG=-all "WINEPREFIX=${PROJECT_BINARY_DIR}/wineprefix")
  endif()

  add_custom_target(belnet-gui
    COMMAND ${YARN} install --frozen-lockfile &&
      ${wine_env} ${YARN} ${GUI_YARN_EXTRA_OPTS} ${GUI_YARN_TARGET}
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}/gui")

  if(APPLE)
  add_custom_target(assemble_gui ALL
  DEPENDS assemble belnet-gui
  COMMAND mkdir "${belnet_app}/Contents/Helpers"
      COMMAND cp -a "${PROJECT_SOURCE_DIR}/gui/release/mac/Belnet-GUI.app" "${belnet_app}/Contents/Helpers/"
      COMMAND mkdir -p "${belnet_app}/Contents/Resources/en.lproj"
      COMMAND cp "${PROJECT_SOURCE_DIR}/contrib/macos/InfoPlist.strings" "${belnet_app}/Contents/Resources/en.lproj/"
      COMMAND cp "${belnet_app}/Contents/Resources/icon.icns" "${belnet_app}/Contents/Helpers/Belnet-GUI.app/Contents/Resources/icon.icns"
      COMMAND cp "${PROJECT_SOURCE_DIR}/contrib/macos/InfoPlist.strings" "${belnet_app}/Contents/Helpers/Belnet-GUI.app/Contents/Resources/en.lproj/"
  COMMAND /usr/libexec/PlistBuddy
    -c "Delete :CFBundleDisplayName"
    -c "Add :LSHasLocalizedDisplayName bool true"
    -c "Add :CFBundleDevelopmentRegion string en"
    -c "Set :CFBundleShortVersionString ${belnet_VERSION}"
    -c "Set :CFBundleVersion ${belnet_VERSION}.${BELNET_APPLE_BUILD}"
    "${belnet_app}/Contents/Helpers/Belnet-GUI.app/Contents/Info.plist"
    )
    
  elseif(WIN32)
    file(MAKE_DIRECTORY "${PROJECT_BINARY_DIR}/gui")
    option(GUI_ZIP_FILE "custom belnet gui for windows from zip file" OFF)
    if(GUI_ZIP_FILE)
      message(STATUS "using custom belnet gui from ${GUI_ZIP_FILE}")
      execute_process(COMMAND ${CMAKE_COMMAND} -E tar xf ${GUI_ZIP_FILE}
        WORKING_DIRECTORY ${PROJECT_BINARY_DIR})
    else()
      add_custom_command(OUTPUT "${PROJECT_BINARY_DIR}/gui/belnet-gui.exe"
        DEPENDS belnet belnet-gui
        # FIXME: we really shouldn't be building inside the source directory but this is npm...
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${PROJECT_SOURCE_DIR}/gui/release/Belnet-GUI_portable.exe"
        "${PROJECT_BINARY_DIR}/gui/belnet-gui.exe"
      )
      add_custom_target(assemble_gui ALL
        DEPENDS ${PROJECT_BINARY_DIR}/gui/belnet-gui.exe)
    endif()
  else()
    message(FATAL_ERROR "Building/bundling the GUI from this repository is not supported on this platform")
  endif()
else()
  message(STATUS "not building gui")
endif()