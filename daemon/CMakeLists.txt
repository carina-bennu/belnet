set(DEFAULT_WITH_BOOTSTRAP ON)
if(APPLE)
  set(DEFAULT_WITH_BOOTSTRAP OFF)
endif()
option(WITH_BOOTSTRAP "build belnet-bootstrap tool" ${DEFAULT_WITH_BOOTSTRAP})
set(exetargets belnet)

if(APPLE)
  add_executable(belnet belnet.swift)
else()
  add_executable(belnet belnet.cpp)
endif()

add_executable(belnet-vpn belnet-vpn.cpp)
enable_lto(belnet belnet-vpn)
list(APPEND exetargets belnet-vpn)

if(WITH_BOOTSTRAP)
  add_executable(belnet-bootstrap belnet-bootstrap.cpp)
  list(APPEND exetargets belnet-bootstrap)
  enable_lto(belnet-bootstrap)
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
  if(should_install)
    if(APPLE)
      install(TARGETS ${exe}
        BUNDLE DESTINATION "${PROJECT_BINARY_DIR}"
        RUNTIME DESTINATION "."
        COMPONENT belnet)
    else()
      install(TARGETS ${exe} RUNTIME DESTINATION bin COMPONENT belnet)
    endif()
  endif()
endforeach()

if(SETCAP)
  install(CODE "execute_process(COMMAND ${SETCAP} cap_net_admin,cap_net_bind_service=+eip ${CMAKE_INSTALL_PREFIX}/bin/belnet)")
endif()
