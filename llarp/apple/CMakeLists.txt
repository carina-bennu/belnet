
# 3.13+ so that we can add link libraries to parent targets
cmake_minimum_required(VERSION 3.13)

if (BUILD_SHARED_LIBS OR NOT BUILD_STATIC_DEPS OR NOT STATIC_LINK)
  message(FATAL_ERROR "macOS builds require a full static build; perhaps use the contrib/macos.sh script to build?")
endif()


find_library(FOUNDATION Foundation REQUIRED)
find_library(NETEXT NetworkExtension REQUIRED)
find_library(COREFOUNDATION CoreFoundation REQUIRED)

target_link_libraries(belnet-util PUBLIC ${FOUNDATION})

target_sources(belnet-platform PRIVATE vpn_platform.cpp vpn_interface.cpp route_manager.cpp context_wrapper.cpp)

add_executable(belnet-extension MACOSX_BUNDLE
  PacketTunnelProvider.m
  DNSTrampoline.m
  )
enable_lto(belnet-extension)
target_link_libraries(belnet-extension PRIVATE
  libbelnet
  ${COREFOUNDATION}
  ${NETEXT})

# Not sure what -fapplication-extension does, but XCode puts it in so...
# -fobjc-arc enables automatic reference counting for objective-C code
# -e _NSExtensionMain because the appex has that instead of a `main` function entry point, of course.
target_compile_options(belnet-extension PRIVATE -fapplication-extension -fobjc-arc)
target_link_options(belnet-extension PRIVATE -fapplication-extension -e _NSExtensionMain)

target_link_libraries(belnet-extension PUBLIC
  libbelnet
  ${COREFOUNDATION}
  ${NETEXT})

set_target_properties(belnet-extension PROPERTIES
  BUNDLE TRUE
  BUNDLE_EXTENSION appex
  MACOSX_BUNDLE_INFO_PLIST ${PROJECT_SOURCE_DIR}/contrib/macos/BelnetExtension.Info.plist.in
  XCODE_PRODUCT_TYPE com.apple.product-type.app-extension
  )

add_custom_command(TARGET belnet-extension
  POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different ${PROJECT_SOURCE_DIR}/contrib/macos/belnet-extension.provisionprofile
  $<TARGET_BUNDLE_DIR:belnet-extension>/Contents/embedded.provisionprofile
  )
