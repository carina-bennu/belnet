

if(BELNET_VERSIONTAG)
  set(VERSIONTAG "${BELNET_VERSIONTAG}")
  configure_file("${CMAKE_CURRENT_SOURCE_DIR}/constants/version.cpp.in" "${CMAKE_CURRENT_BINARY_DIR}/constants/version.cpp" @ONLY)
  else()
  set(VERSIONTAG "${GIT_VERSION}")
  set(GIT_INDEX_FILE "${PROJECT_SOURCE_DIR}/.git/index")
  find_package(Git)
  if(EXISTS "${GIT_INDEX_FILE}" AND ( GIT_FOUND OR Git_FOUND) )
      message(STATUS "Found Git: ${GIT_EXECUTABLE}")
      set(genversion_args "-DGIT=${GIT_EXECUTABLE}")
      foreach(v belnet_VERSION belnet_VERSION_MAJOR belnet_VERSION_MINOR belnet_VERSION_PATCH RELEASE_MOTTO)
          list(APPEND genversion_args "-D${v}=${${v}}")
      endforeach()

      add_custom_command(
          OUTPUT            "${CMAKE_CURRENT_BINARY_DIR}/constants/version.cpp"
          COMMAND           "${CMAKE_COMMAND}"
          ${genversion_args}
          "-D" "SRC=${CMAKE_CURRENT_SOURCE_DIR}/constants/version.cpp.in"
          "-D" "DEST=${CMAKE_CURRENT_BINARY_DIR}/constants/version.cpp"
          "-P" "${CMAKE_CURRENT_LIST_DIR}/GenVersion.cmake"
          DEPENDS           "${CMAKE_CURRENT_SOURCE_DIR}/constants/version.cpp.in"
          "${GIT_INDEX_FILE}")
  else()
    configure_file("${CMAKE_CURRENT_SOURCE_DIR}/constants/version.cpp.in" "${CMAKE_CURRENT_BINARY_DIR}/constants/version.cpp" @ONLY)
  endif()
endif()

  if(WIN32)
  foreach(exe IN ITEMS belnet belnet-vpn belnet-bootstrap)
    set(belnet_EXE_NAME "${exe}.exe")
    configure_file("${CMAKE_CURRENT_SOURCE_DIR}/win32/version.rc.in" "${CMAKE_BINARY_DIR}/${exe}.rc" @ONLY)
    set_property(SOURCE "${CMAKE_BINARY_DIR}/${exe}.rc" PROPERTY GENERATED 1)
  endforeach()
endif()

add_custom_target(genversion_cpp DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/constants/version.cpp")
if(WIN32)
  add_custom_target(genversion_rc DEPENDS "${CMAKE_BINARY_DIR}/belnet.rc" "${CMAKE_BINARY_DIR}/belnet-vpn.rc" "${CMAKE_BINARY_DIR}/belnet-bootstrap.rc")
else()
  add_custom_target(genversion_rc)
endif()
add_custom_target(genversion DEPENDS genversion_cpp genversion_rc)
