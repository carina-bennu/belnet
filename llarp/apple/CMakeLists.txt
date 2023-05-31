
# 3.13+ so that we can add link libraries to parent targets
cmake_minimum_required(VERSION 3.13)

if (BUILD_SHARED_LIBS OR NOT BUILD_STATIC_DEPS OR NOT STATIC_LINK)
  message(FATAL_ERROR "macOS builds require a full static build; perhaps use the contrib/mac.sh script to build?")
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

# -fobjc-arc enables automatic reference counting for objective-C code
# -e _NSExtensionMain because the appex has that instead of a `main` function entry point, of course.
target_compile_options(belnet-extension PRIVATE -fobjc-arc)
if(MACOS_SYSTEM_EXTENSION)
  target_compile_definitions(belnet-extension PRIVATE MACOS_SYSTEM_EXTENSION)
  target_compile_definitions(belnet-util PUBLIC MACOS_SYSTEM_EXTENSION)
else()
  target_link_options(belnet-extension PRIVATE -e _NSExtensionMain)
endif()

if(MACOS_SYSTEM_EXTENSION)
    set(bundle_ext systemextension)
    set(product_type com.apple.product-type.system-extension)
else()
    set(bundle_ext appex)
    set(product_type com.apple.product-type.app-extension)
endif()

target_link_libraries(belnet-extension PRIVATE
  belnet-amalgum
  ${COREFOUNDATION}
  ${NETEXT})

set_target_properties(belnet-extension PROPERTIES
  BUNDLE TRUE
  BUNDLE_EXTENSION ${bundle_ext}
  OUTPUT_NAME org.belnet.network-extension
  MACOSX_BUNDLE_INFO_PLIST ${PROJECT_SOURCE_DIR}/contrib/macos/belnet-extension.Info.plist.in
  XCODE_PRODUCT_TYPE ${product_type}
  )

if(CODESIGN AND CODESIGN_EXT_PROFILE)
  add_custom_command(TARGET belnet-extension
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      ${CODESIGN_EXT_PROFILE}
      $<TARGET_BUNDLE_DIR:belnet-extension>/Contents/embedded.provisionprofile
  )
endif()
