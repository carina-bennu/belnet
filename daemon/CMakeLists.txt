set(DEFAULT_WITH_BOOTSTRAP ON)
if(APPLE)
  set(DEFAULT_WITH_BOOTSTRAP OFF)
endif()
option(WITH_BOOTSTRAP "build belnet-bootstrap tool" ${DEFAULT_WITH_BOOTSTRAP})

add_executable(belnet-vpn belnet-vpn.cpp)
if(APPLE)
  add_executable(belnet belnet.swift)
  enable_lto(belnet)
else()
  add_executable(belnet belnet.cpp)
  enable_lto(belnet belnet-vpn)

  if(WITH_BOOTSTRAP)
    add_executable(belnet-bootstrap belnet-bootstrap.cpp)
    enable_lto(belnet-bootstrap)
  endif()
endif()


if(TRACY_ROOT)
  target_sources(belnet PRIVATE ${TRACY_ROOT}/TracyClient.cpp)
endif()

set(should_install ON)
set(SETCAP)

if(CMAKE_SYSTEM_NAME MATCHES "Linux")
  option(WITH_SETCAP "use setcap when installing" ON)
  if(WITH_SETCAP)
    find_program(SETCAP NAMES setcap HINTS /sbin /usr/sbin)
    if(SETCAP)
      message(STATUS "Found setcap binary: ${SETCAP}")
    else()
      message(WARNING "cannot find setcap binary you will not be able use the install targets unless you use -DWITH_SETCAP=OFF")
      set(should_install OFF)
    endif()
  endif()
endif()

if(WITH_BOOTSTRAP)
  target_link_libraries(belnet-bootstrap PUBLIC cpr::cpr)
  if(NOT WIN32)
    find_package(OpenSSL REQUIRED)
    # because debian sid's curl doesn't link against openssl for some godawful cursed reason
    target_link_libraries(belnet-bootstrap PUBLIC OpenSSL::SSL OpenSSL::Crypto)
  endif()
endif()

set(exetargets belnet belnet-vpn)
if(WITH_BOOTSTRAP)
  list(APPEND exetargets belnet-bootstrap)
endif()

foreach(exe ${exetargets})
  if(WIN32 AND NOT MSVC_VERSION)
    target_sources(${exe} PRIVATE ${CMAKE_BINARY_DIR}/${exe}.rc)
    target_link_libraries(${exe} PRIVATE -static-libstdc++ -static-libgcc --static -Wl,--pic-executable,-e,mainCRTStartup,--subsystem,console:5.00)
    target_link_libraries(${exe} PRIVATE ws2_32 iphlpapi)
  elseif(CMAKE_SYSTEM_NAME MATCHES "FreeBSD")
    target_link_directories(${exe} PRIVATE /usr/local/lib)
  endif()
  target_link_libraries(${exe} PUBLIC libbelnet)
  target_include_directories(${exe} PUBLIC "${PROJECT_SOURCE_DIR}")
  target_compile_definitions(${exe} PRIVATE -DVERSIONTAG=${GIT_VERSION_REAL})
  add_log_tag(${exe})
  if(should_install)
    if(APPLE)
      install(TARGETS ${exe} BUNDLE DESTINATION "${PROJECT_BINARY_DIR}" COMPONENT belnet)
    else()
      install(TARGETS ${exe} RUNTIME DESTINATION bin COMPONENT belnet)
    endif()
  endif()
endforeach()

if(APPLE)

  set(CODESIGN_APP "" CACHE STRING "codesign the macos app using this key identity")
  set(CODESIGN_APPEX "${CODESIGN_APP}" CACHE STRING "codesign the internal extension using this key identity; defaults to CODESIGN_APP if empty")

  set(mac_icon ${CMAKE_CURRENT_BINARY_DIR}/belnet.icns)
  add_custom_command(OUTPUT ${mac_icon}
    COMMAND ${PROJECT_SOURCE_DIR}/contrib/macos/mk-icns.sh ${PROJECT_SOURCE_DIR}/contrib/belnet.svg ${mac_icon}
    DEPENDS ${PROJECT_SOURCE_DIR}/contrib/belnet.svg ${PROJECT_SOURCE_DIR}/contrib/macos/mk-icns.sh)
  add_custom_target(icons DEPENDS ${mac_icon})
  add_dependencies(belnet icons belnet-extension)
  add_custom_command(TARGET belnet
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${PROJECT_SOURCE_DIR}/contrib/bootstrap/mainnet.signed
        $<TARGET_BUNDLE_DIR:belnet-extension>/Contents/Resources/bootstrap.signed
    COMMAND mkdir -p $<TARGET_BUNDLE_DIR:belnet>/Contents/PlugIns
    COMMAND cp -a $<TARGET_BUNDLE_DIR:belnet-extension> $<TARGET_BUNDLE_DIR:belnet>/Contents/PlugIns/
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${PROJECT_SOURCE_DIR}/contrib/macos/belnet.provisionprofile
        $<TARGET_BUNDLE_DIR:belnet>/Contents/embedded.provisionprofile
    )

  set_target_properties(belnet
    PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_INFO_STRING "Belnet IP Packet Onion Router"
    MACOSX_BUNDLE_BUNDLE_NAME "Belnet"
    MACOSX_BUNDLE_BUNDLE_VERSION "${belnet_VERSION}"
    MACOSX_BUNDLE_LONG_VERSION_STRING "${belnet_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${belnet_VERSION_MAJOR}.${belnet_VERSION_MINOR}"
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.beldex-project.belnet"
    MACOSX_BUNDLE_INFO_PLIST "${PROJECT_SOURCE_DIR}/contrib/macos/Info.plist.in"
    MACOSX_BUNDLE_ICON_FILE "${mac_icon}"
    MACOSX_BUNDLE_COPYRIGHT "© 2021, The Beldex Project")
  if (CODESIGN_APP AND CODESIGN_APPEX)
    message(STATUS "codesigning with ${CODESIGN_APP} (app) ${CODESIGN_APPEX} (appex)")
    set(SIGN_TARGET "${CMAKE_CURRENT_BINARY_DIR}/belnet.app")
    configure_file(
      "${PROJECT_SOURCE_DIR}/contrib/macos/sign.sh.in"
      "${PROJECT_BINARY_DIR}/sign.sh"
      @ONLY)
    add_custom_target(
      sign
      DEPENDS "${PROJECT_BINARY_DIR}/sign.sh" belnet belnet-extension
      COMMAND "${PROJECT_BINARY_DIR}/sign.sh"
      )
  else()
    message(WARNING "Not codesigning: CODESIGN_APP (=${CODESIGN_APP}) and/or CODESIGN_APPEX (=${CODESIGN_APPEX}) are not set")
    add_custom_target(
      sign
      DEPENDS belnet belnet-extension
      COMMAND "true")
  endif()
endif()

if(SETCAP)
  install(CODE "execute_process(COMMAND ${SETCAP} cap_net_admin,cap_net_bind_service=+eip ${CMAKE_INSTALL_PREFIX}/bin/belnet)")
endif()
