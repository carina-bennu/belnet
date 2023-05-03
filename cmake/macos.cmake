if(NOT APPLE)
  return()
endif()


option(MACOS_SYSTEM_EXTENSION
  "Build the network extension as a system extension rather than a plugin.  This must be ON for non-app store release builds, and must be OFF for dev builds and Mac App Store distribution builds"
  OFF)
option(CODESIGN "codesign the resulting app and extension" ON)
set(CODESIGN_ID "" CACHE STRING "codesign the macos app using this key identity; if empty we'll try to guess")
set(default_profile_type "dev")
if(MACOS_SYSTEM_EXTENSION)
  set(default_profile_type "release")
endif()
set(CODESIGN_PROFILE "${PROJECT_SOURCE_DIR}/contrib/macos/belnet.${default_profile_type}.provisionprofile" CACHE FILEPATH
  "Path to a .provisionprofile to use for the main app")
set(CODESIGN_EXT_PROFILE "${PROJECT_SOURCE_DIR}/contrib/macos/belnet-extension.${default_profile_type}.provisionprofile" CACHE FILEPATH
  "Path to a .provisionprofile to use for the belnet extension")

if(CODESIGN AND NOT CODESIGN_ID)
  if(MACOS_SYSTEM_EXTENSION)
    set(codesign_cert_pattern "Developer ID Application")
  else()
    set(codesign_cert_pattern "Apple Development")
  endif()
  execute_process(
    COMMAND security find-identity -v -p codesigning
    COMMAND sed -n "s/^ *[0-9][0-9]*)  *\\([A-F0-9]\\{40\\}\\)  *\"\\(${codesign_cert_pattern}.*\\)\"\$/\\1 \\2/p"
    RESULT_VARIABLE find_id_exit_code
    OUTPUT_VARIABLE find_id_output)
  if(NOT find_id_exit_code EQUAL 0)
    message(FATAL_ERROR "Finding signing identities with security find-identity failed; try specifying an id using -DCODESIGN_ID=...")
  endif()

  string(REGEX MATCHALL "(^|\n)[0-9A-F]+" find_id_sign_id "${find_id_output}")
  if(NOT find_id_sign_id)
    message(FATAL_ERROR "Did not find any \"${codesign_cert_pattern}\" identity; try specifying an id using -DCODESIGN_ID=...")
  endif()
  if (find_id_sign_id MATCHES ";")
    message(FATAL_ERROR "Found multiple \"${codesign_cert_pattern}\" identities:\n${find_id_output}\nSpecify an identify using -DCODESIGN_ID=...")
  endif()
  set(CODESIGN_ID "${find_id_sign_id}" CACHE STRING "" FORCE)
endif()

if(CODESIGN)
  message(STATUS "Codesigning using ${CODESIGN_ID}")

  if (NOT MACOS_NOTARIZE_USER AND NOT MACOS_NOTARIZE_PASS AND NOT MACOS_NOTARIZE_ASC AND EXISTS "$ENV{HOME}/.notarization.cmake")
    message(STATUS "Loading notarization info from ~/.notarization.cmake")
    include("$ENV{HOME}/.notarization.cmake")
  endif()

  if (MACOS_NOTARIZE_USER AND MACOS_NOTARIZE_PASS AND MACOS_NOTARIZE_ASC)
    message(STATUS "Enabling notarization with account ${MACOS_NOTARIZE_ASC}/${MACOS_NOTARIZE_USER}")
  else()
    message(WARNING "You have not set one or more of MACOS_NOTARIZE_USER, MACOS_NOTARIZE_PASS, MACOS_NOTARIZE_ASC: notarization will fail; see contrib/macos/README.txt")
  endif()

else()
  message(WARNING "Codesigning disabled; the resulting build will not run on most macOS systems")
endif()

foreach(prof IN ITEMS CODESIGN_PROFILE CODESIGN_EXT_PROFILE)
  if(NOT ${prof})
    message(WARNING "Missing a ${prof} provisioning profile: Apple will most likely log an uninformative error message to the system log and then kill harmless kittens if you try to run the result")
  elseif(NOT EXISTS "${${prof}}")
    message(FATAL_ERROR "Provisioning profile ${${prof}} does not exist; fix your -D${prof} path")
  endif()
endforeach()
message(STATUS "Using ${CODESIGN_PROFILE} app provisioning profile")
message(STATUS "Using ${CODESIGN_EXT_PROFILE} extension provisioning profile")

set(belnet_installer "${PROJECT_BINARY_DIR}/Belnet ${PROJECT_VERSION}")
set(belnet_app "${belnet_installer}/Belnet.app")


if(MACOS_SYSTEM_EXTENSION)
  set(belnet_ext_dir Contents/Library/SystemExtensions)
else()
  set(belnet_ext_dir Contents/PlugIns)
endif()

if(CODESIGN)
  if(MACOS_SYSTEM_EXTENSION)
    set(BELNET_ENTITLEMENTS_TYPE sysext)
    set(notarize_py_is_sysext True)
  else()
    set(BELNET_ENTITLEMENTS_TYPE plugin)
    set(notarize_py_is_sysext False)
  endif()

  configure_file(
    "${PROJECT_SOURCE_DIR}/contrib/macos/sign.sh.in"
    "${PROJECT_BINARY_DIR}/sign.sh"
    @ONLY)

  add_custom_target(
    sign
    DEPENDS "${PROJECT_BINARY_DIR}/sign.sh"
    COMMAND "${PROJECT_BINARY_DIR}/sign.sh"
    )

  if(MACOS_NOTARIZE_USER AND MACOS_NOTARIZE_PASS AND MACOS_NOTARIZE_ASC)
    configure_file(
      "${PROJECT_SOURCE_DIR}/contrib/macos/notarize.py.in"
      "${PROJECT_BINARY_DIR}/notarize.py"
      @ONLY)
    add_custom_target(
      notarize
      DEPENDS "${PROJECT_BINARY_DIR}/notarize.py" sign
      COMMAND "${PROJECT_BINARY_DIR}/notarize.py"
      )
  else()
    message(WARNING "You have not set one or more of MACOS_NOTARIZE_USER, MACOS_NOTARIZE_PASS, MACOS_NOTARIZE_ASC: notarization disabled")
  endif()
else()
  add_custom_target(sign COMMAND "true")
  add_custom_target(notarize DEPENDS sign COMMAND "true")
endif()


set(mac_icon "${PROJECT_BINARY_DIR}/belnet.icns")
add_custom_command(OUTPUT "${mac_icon}"
  COMMAND ${PROJECT_SOURCE_DIR}/contrib/macos/mk-icns.sh ${PROJECT_SOURCE_DIR}/contrib/belnet-mac.svg "${mac_icon}"
  DEPENDS ${PROJECT_SOURCE_DIR}/contrib/belnet.svg ${PROJECT_SOURCE_DIR}/contrib/macos/mk-icns.sh)
add_custom_target(icon DEPENDS "${mac_icon}")

if(BUILD_PACKAGE)
  add_executable(seticon "${PROJECT_SOURCE_DIR}/contrib/macos/seticon.swift")
  add_custom_command(OUTPUT "${belnet_installer}.dmg"
    DEPENDS notarize seticon
    COMMAND create-dmg
      --volname "Belnet ${PROJECT_VERSION}"
      --volicon belnet.icns
      #--background ... FIXME
      --text-size 16
      --icon-size 128
      --window-size 500 300
      --icon Belnet.app 100 100
      --hide-extension Belnet.app
      --app-drop-link 350 100
      --eula "${PROJECT_SOURCE_DIR}/LICENSE"
      --no-internet-enable
      "${belnet_installer}.dmg"
      "${belnet_installer}"
      COMMAND ./seticon belnet.icns "${belnet_installer}.dmg"
  )
  add_custom_target(package DEPENDS "${belnet_installer}.dmg")
endif()


# Called later to set things up, after the main belnet targets are set up
function(macos_target_setup)

  if(MACOS_SYSTEM_EXTENSION)
    target_compile_definitions(belnet PRIVATE MACOS_SYSTEM_EXTENSION)
  endif()

  set_target_properties(belnet
    PROPERTIES
    OUTPUT_NAME Belnet
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_INFO_STRING "Belnet IP Packet Onion Router"
    MACOSX_BUNDLE_BUNDLE_NAME "Belnet"
    MACOSX_BUNDLE_BUNDLE_VERSION "${belnet_VERSION}"
    MACOSX_BUNDLE_LONG_VERSION_STRING "${belnet_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${belnet_VERSION_MAJOR}.${belnet_VERSION_MINOR}"
    MACOSX_BUNDLE_GUI_IDENTIFIER "org.belnet"
    MACOSX_BUNDLE_INFO_PLIST "${PROJECT_SOURCE_DIR}/contrib/macos/belnet.Info.plist.in"
    MACOSX_BUNDLE_COPYRIGHT "© 2022, The Beldex Project"
  )

  add_custom_target(copy_bootstrap
    DEPENDS belnet-extension
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${PROJECT_SOURCE_DIR}/contrib/bootstrap/mainnet.signed
      $<TARGET_BUNDLE_DIR:belnet-extension>/Contents/Resources/bootstrap.signed
  )



  add_dependencies(belnet belnet-extension icon)


  if(CODESIGN_PROFILE)
    add_custom_target(copy_prov_prof
      DEPENDS belnet
      COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CODESIGN_PROFILE}
        $<TARGET_BUNDLE_DIR:belnet>/Contents/embedded.provisionprofile
    )
  else()
    add_custom_target(copy_prov_prof COMMAND true)
  endif()

  add_custom_target(assemble ALL
    DEPENDS belnet belnet-extension icon copy_prov_prof copy_bootstrap
    COMMAND rm -rf "${belnet_app}"
    COMMAND mkdir -p "${belnet_installer}"
    COMMAND cp -a $<TARGET_BUNDLE_DIR:belnet> "${belnet_app}"
    COMMAND mkdir -p "${belnet_app}/${belnet_ext_dir}"
    COMMAND cp -a $<TARGET_BUNDLE_DIR:belnet-extension> "${belnet_app}/${belnet_ext_dir}/"
    COMMAND mkdir -p "${belnet_app}/Contents/Resources"
    COMMAND cp -a "${mac_icon}" "${belnet_app}/Contents/Resources/icon.icns"
  )

  if(CODESIGN AND BUILD_GUI)
    add_dependencies(sign assemble_gui)
  elseif(CODESIGN)
    add_dependencies(sign assemble)
  endif()
endfunction()